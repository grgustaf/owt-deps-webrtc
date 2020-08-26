import os
import sys

argc = len(sys.argv)
if argc < 2:
    print("usage: python frames.py LOGFILE")
    sys.exit(1)

LOGFILE = sys.argv[1]

if not os.path.isfile(LOGFILE):
    print("logfile " + LOGFILE + " does not exist")
    exit(1)

stats = {
    # number of times FEC recovered a packet before we even knew it was missing
    'fec_early_recoveries': 0,
    # number of times FEC recovered a packet we did know was missing
    'fec_missing_recoveries': 0,
    # number of times the regular packet showed up later that FEC had already recovered
    'fec_duplicates': 0,
    # number of times an RTX packet was actually needed
    'rtx_recoveries': 0,
    # number of times an RTX packet showed up later that FEC had already recovered
    'rtx_after_fec_duplicates': 0,
    # number of times more than one RTX packet was received for the same packet
    'rtx_duplicate_recoveries': 0,

    'total_missing': 0,

    # errors of no great consequence
    'short_lines': 0,
    'time_missing': 0,
    'video_unhandled': 0,
    'video_badid': 0,
    'video_missing': 0,
    'invalid_fec': 0
}

# sequence number of video packet just recovered via RTX
rtx_packet = -1
# time it had taken to receive the RTX packet since it was found missing
rtx_waited = 0
rtx_badid = 0

# array of packets recently recovered via FEC
fec_recovered = []
fec_resolving = True

class VideoPacket:
    def __init__(self, timestamp, seq_num, uid, missing, rtx, fec):
        self.timestamp = timestamp
        self.seq_num = seq_num
        self.uid = uid
        self.missing = missing
        self.was_missing = missing
        # change from bool to count of # of times RTX received
        self.rtx = rtx
        self.rtx_recovery = None
        self.fec = fec
        self.fec_early = False
        self.fec_recovery = None
        self.fec_packets = []

    def correlate_with_fec_packet(self, fec_packet):
        if fec_packet not in self.fec_packets:
            self.fec_packets.append(fec_packet)

    def unparse(self):
        print("Video Packet #" + str(self.uid) + ":")
        print("  sequence number:", self.seq_num)
        if self.missing:
            print("  found missing at", self.timestamp)
        else:
            print("  received at ", self.timestamp)
        if self.rtx:
            print("  was missing at", self.timestamp)
            print("  recovered by RTX at ", self.rtx_recovery)
            if self.rtx > 1:
                print('  received {:d} duplicate RTX packets'.format(self.rtx - 1 ))
        if self.fec:
            print("  recovered by FEC")

    def unparse_short(self):
        extras = ''
        if len(self.fec_packets) > 0:
            extras += 'F' + str(len(self.fec_packets)) + ' '
        else:
            extras += '   '
        if self.missing:
            extras += ' MISSING'
        extras += ' *' if self.fec and not self.fec_early else '  '
        extras += ' FEC recovered at {:07.3f}'.format(self.fec_recovery) if self.fec else ' ' * 25
        if self.rtx:
            extras += '    RTX received at {:07.3f}'.format(self.rtx_recovery)
            if self.rtx > 1:
                extras += ' ({:d}x)'.format(self.rtx)
        time = ' L O S T '
        if not self.was_missing:
            time = '[{:07.3f}]'.format(self.timestamp)
        print('  {:s} {:d} (#{:d}) {:s}'.format(time, self.seq_num, self.uid, extras))

class FecPacket:
    def __init__(self, timestamp, seq_num, protects):
        self.timestamp = timestamp
        self.seq_num = seq_num
        self.protects = protects

    def unparse(self):
        protect = ' '.join([str(x) for x in self.protects])
        time = '[{:07.3f}]'.format(self.timestamp)
        print('  {:s} FEC Packet #{:d}: protects {:s}'.format(time, self.seq_num, protect))

# indexed by video packet id, value is list of protecting FEC packets
map_packet_to_fec = {}

packets = {}

def parse_uid(word):
    # expecting format (#123), otherwise return 0
    if len(word) < 4 or word[0:2] != '(#' or word[-1] != ')':
        return 0

    return int(word[2:-1])

# dictionary mapping sequence numbers to list of packets with that sequence number
save_packets = {}

def correlate_fec(video_packet):
    seq_num = video_packet.seq_num

    for fec_packet in map_packet_to_fec.get(seq_num, []):
        if fec_packet.timestamp < video_packet.timestamp - 1:
            # not the right one, skip
            continue

        # for all applicable FEC packets, use this opportunity to try to correlate them
        # to all the other protected packets as well

        for num in fec_packet.protects:
            # find the video packet and link this FEC packet to it
            packet = packets.get(num, None)
            if not packet:
                # this can happen if more than one packet is recovered by FEC in the same pass
                # it will probably get processed right after this, can probably be ignored
                continue
            if packet.timestamp < fec_packet.timestamp - 1 or packet.timestamp > fec_packet.timestamp + 1:
                # this can also happen if another packet hasn't been recovered yet, like above
                continue

            packet.correlate_with_fec_packet(fec_packet)

        # we can't remove the packet from the map because we don't know which one
        # actually resolved it; could fix this with extra logging detail
        # map_packet_to_fec[seq_num].remove(fec_packet)

def handle_video(words, timestamp):
    global stats, rtx_packet, fec_recovered, fec_resolving

    # look for format 96-18497 (#1)
    if len(words) < 2:
        stats['video_unhandled'] += 1
        return

    fields = words[0].split('-')
    if len(fields) != 2:
        stats['video_badid'] += 1
        return

    rtp_type, seq_str = fields
    seq_num = int(seq_str)

    missing = False
    if words[1] == "MISSING":
        missing = True
    
    uid = parse_uid(words[1])

    # overwrite if packet number never used or very old
    if seq_num not in packets or packets[seq_num].timestamp < timestamp - 10:
        # save the evicted packet
        if seq_num in packets:
            if seq_num in save_packets:
                save_packets[seq_num].append(packets[seq_num])
            else:
                save_packets[seq_num] = [packets[seq_num]]

        if seq_num in fec_recovered:
            # FEC recovered the packet before we even realized it was missing 
            packets[seq_num] = VideoPacket(timestamp, seq_num, uid, False, False, True)
            packets[seq_num].fec = True
            packets[seq_num].fec_early = True
            packets[seq_num].fec_recovery = timestamp
            stats['fec_early_recoveries'] += 1
            fec_recovered.remove(seq_num)

            correlate_fec(packets[seq_num])

            # if more recoveries come in before we process these, maybe we should clear
            # the list? not sure if it will happen, so tracking like this
            fec_resolving = True
            return

        packets[seq_num] = VideoPacket(timestamp, seq_num, uid, missing, False, False)
        return

    old_packet = packets.get(seq_num)
    if old_packet.timestamp > timestamp - 1:
        # packet from within the last second

        # handle RTX packets
        if seq_num == rtx_packet:
            if old_packet.missing:
                stats['rtx_recoveries'] += 1
                old_packet.uid = uid
                old_packet.missing = False
                old_packet.rtx = 1
                old_packet.rtx_recovery = timestamp
                waited = int((timestamp - old_packet.timestamp) * 1000)
                if waited < rtx_waited - 2 or waited > rtx_waited + 2:
                    print(waited, rtx_waited)
                    raise Exception("mismatch in wait time for RTX recovered packet " +
                        str(uid))
                rtx_packet = -1
                return

            if not old_packet.fec and old_packet.rtx == 0:
                raise Exception('received RTX for packet that was never missing: ' + str(uid))

            if old_packet.fec:
                # RTX packet showed up after FEC already recovered
                if old_packet.rtx == 0:
                    old_packet.rtx_recovery = timestamp

                stats['rtx_after_fec_duplicates'] += 1
                # TODO: could track stale RTX arrival time

            if old_packet.rtx > 0:
                stats['rtx_duplicate_recoveries'] += 1

            old_packet.rtx += 1
            rtx_packet = -1
            return

        if seq_num in fec_recovered:
            if old_packet.missing:
                # packet was found missing and then recovered by FEC
                stats['fec_missing_recoveries'] += 1
                old_packet.uid = uid
                old_packet.missing = False
                old_packet.fec = True
                old_packet.fec_recovery = timestamp

                fec_recovered.remove(seq_num)
                fec_resolving = True
                return

            # don't expect to see FEC recovery after FEC or RTX recovery
            raise Exception('unexpected duplicate FEC recovery')

        if old_packet.fec:
            # original packet showed up (or out of order or duplicated by router)
            stats['fec_duplicates'] += 1
            return
            # TODO: could record duplicate time and look at average time between fec and duplicate arrival

        # with out of order packet arrival, we could have regular packet show up after RTX?

        # packet seems too recent to be from wraparound, investigate
        old_packet.unparse()
        msg = ("packet from " + str(old_packet.timestamp) + " with same id " +
            str(seq_num) + " too recent (now " + str(timestamp) + ")")
        raise Exception(msg)

    if seq_num == rtx_packet:
        print("Found RTX video packet")

    if words[1] == "MISSING":
        print("Found missing packet")

def handle_rtx(words, timestamp):
    global rtx_badid, rtx_packet, rtx_waited

    # expecting "for 96-15803 arrived after 62 ms"
    if len(words) < 6 or words[0] != 'for' or words[2] != 'arrived':
        return

    fields = words[1].split('-')
    if len(fields) != 2:
        rtx_badid += 1
        return

    rtc_type, seq_str = fields
    seq_num = int(seq_str)

    if rtx_packet != -1:
        raise Exception("unprocessed RTX packet found: " + str(rtx_packet))

    rtx_packet = seq_num
    rtx_waited = int(words[4])

def handle_fec_recovery(remain, timestamp):
    global fec_recovered, fec_resolving

    if remain[:18] != "recovered by FEC: ":
        return

    if fec_resolving and len(fec_recovered):
        print(timestamp, fec_recovered)
        raise Exception("not all FEC recovered packets were resolved")

    media_packet = int(remain[18:])
    fec_recovered.append(media_packet)
    fec_resolving = False

def handle_fec(remain, timestamp):
    # format of remain: 3217 protects 1238 1240 1242 1244 1246
    words = remain.split(' ')
    if len(words) < 3 or words[1] != 'protects':
        stats['invalid_fec'] += 1
        return

    protect_list = [int(x) for x in words[2:]]
    fec_packet = FecPacket(timestamp, int(words[0]), protect_list)
    for seq_num in protect_list:
        packet_list = map_packet_to_fec.get(seq_num, [])
        packet_list.append(fec_packet)
        map_packet_to_fec[seq_num] = packet_list

CACHED_FRAMES = 128
# frames being considered to find gaps
frame_cache = []
# frames old enough that we are sure they've been 
final_frames = []

def add_frame(packet_range):
    if len(frame_cache) == CACHED_FRAMES:
        final_frames.append(frame_cache.pop(0))

    for i in range(len(frame_cache)):
        # check if frame precedes current frame 
        if packet_range[0] < frame_cache[i][0]:
            # make sure it's not because of wraparound, e,g #25 < #65500
            if packet_range[0] + 32768 > frame_cache[i][0]:
                if packet_range[1] >= frame_cache[i][0]:
                    raise Exception("packet ranges overlap, appears to be a time warp")
                frame_cache.insert(i, packet_range)
                return

    frame_cache.append(packet_range)

def handle_assembled(remain, timestamp):
    # format is [GAP] 37 (1355) 1044-1055
    gap = remain[:6] == "[GAP] "
    if not gap and remain[:6] != "      ":
        return

    words = remain[6:].split()
    # skip extra leading space if present
#    if len(words) == 4:
#        words = words[1:]

    count = total = range_str = None
    if len(words) > 3:
        # might be a wraparound case where all frames are listed e.g.
        #   65534 65535 0 1 2 3
        if "65535" not in words or "0" not in words:
            raise Exception('unexpected number of words')

        # fix up to match format expected below
        count = words[0]
        total = words[1]
        range_str = words[2] + "-" + words[-1]
    else:
        count, total, range_str = words

    if total[0] != '(' or total[-1] != ')':
        raise Exception('unexpected arg format')

    count = int(count)
    total = int(total[1:-1])
    packet_range = [int(x) for x in range_str.split('-')]
    packet_range.append(timestamp)

    add_frame(packet_range)

with open(LOGFILE) as log:
    for line in log:
        line = line.strip()
        words = line.split(' ')

        if len(words) < 2:
            stats['short_lines'] += 1
            continue

        timestr = words[0]
        if len(timestr) < 9 or timestr[0] != '[' or timestr[4] != '.' or timestr[8] != ']':
            # not a valid log line
            stats['time_missing'] += 1
            continue

        timestamp = float(timestr[1:8])

        command = words[1]
        if command == "Video":
            handle_video(words[2:], timestamp)

        if command == "RTX":
            handle_rtx(words[2:], timestamp)

        if command == "Assembled":
            handle_assembled(line[20:], timestamp)

        if command == "Packet":
            handle_fec_recovery(line[17:], timestamp)

        if command == "FEC":
            handle_fec(line[21:], timestamp)

for key in packets.keys():
    if key in save_packets:
        save_packets[key].append(packets[key])
    else:
        save_packets[key] = [packets[key]]

final_frames.extend(frame_cache)

count = 1

def unparse_frame(first, last, timestamp, label=''):
    num_packets = last - first + 1
    if num_packets < 0:
        num_packets += 65536
    print('[{:07.3f}] Frame #{:d}: {:d} - {:d} ({:d})  {:s}'.format(timestamp, count, first, last,
        num_packets, label))

prev_first, prev_last, prev_timestamp = final_frames[0]
unparse_frame(prev_first, prev_last, prev_timestamp)
last_ordered = 1
last_time = prev_timestamp

frames_by_time = sorted(final_frames, key=lambda x: x[2])

def catch_up():
    while stalled_frames[0] == last_ordered + 1:
        last_ordered += 1
        stalled_frames = stalled_frames[1:]

stalled = False
# index of most recent frame assembled later than following frames
late_frame = -1
# list of all the late frames, indexes into final_frames
late_frames = []
# max ms another frame was stalled for this one
max_stall = 0
stall_count = 0
total_stall_count = 0

for first, last, timestamp in final_frames[1:]:
    if prev_last + 1 != first and (prev_last != 65535 or first != 0):
        base = prev_last
        if base > first:
            base -= 65536
        print('Gap of', first - base, 'packets.')

    count += 1
    stall = ''
    if timestamp < prev_timestamp:
        stall_time = int(prev_timestamp * 1000) - int(timestamp * 1000)
        stall = "Stalled by {:0d} ms".format(stall_time)
        if not stalled:
            # subtract one to get to previous frame and one to get 0-indexed number
            late_frame = count - 2
            stalled = True
        if stall_time > max_stall:
            max_stall = stall_time
        stall_count += 1
    else:
        prev_timestamp = timestamp
        if stalled:
            late_frames.append({
                'frame_index': late_frame,
                'max_stall': max_stall,
                'stall_count': stall_count
            })
            total_stall_count += stall_count
            stalled = False
            max_stall = 0
            stall_count = 0
    unparse_frame(first, last, timestamp, stall)

    prev_first, prev_last = first, last

def time_matches_frame(time_packet, time_frame):
    # time_packet is the time the considered packet arrived
    # time_frame is the time the frame was assembled
    # return true if this packet is likely the right one associated with this frame
    # for now, assume if it arrived in the 10s before the frame was assembled it's right
    if time_packet <= time_frame and time_packet + 10 > time_frame:
        return True
    return False

for late in late_frames:
    first, last, timestamp = final_frames[late['frame_index']]

    fmt = '\nDebugging late frame #{:d} ({:d} packets): stalled {:d} frames (max {:d} ms)'
    print(fmt.format(late['frame_index'] + 1, last - first + 1, late['stall_count'], late['max_stall']))
    fec_packets = []
    for seq_num in range(first, last + 1):
        found = False
        for packet in save_packets[seq_num]:
            if time_matches_frame(packet.timestamp, timestamp):
                found = True
                break
        if not found:
            print(timestamp, seq_num)
            # this can happen on very late frames, so don't raise an exception for now
            raise Exception("packet not found that matches frame - increase match threshold?")
        packet.unparse_short()
        for fec_packet in packet.fec_packets:
            if fec_packet not in fec_packets:
                fec_packets.append(fec_packet)

    # list all FEC packets associated with this frame
    sorted_packets = sorted(fec_packets, key=lambda x: x.seq_num)
    for fec_packet in sorted_packets:
        fec_packet.unparse()

print('\nEarly FEC Recoveries:', stats['fec_early_recoveries'])
print('Packets that showed up normally after recovery:', stats['fec_duplicates'])
print('Missing FEC Recoveries:', stats['fec_missing_recoveries'])
print('Missing RTX Recoveries:', stats['rtx_recoveries'])
print('Packets that showed up via RTX after FEC recovery:', stats['rtx_after_fec_duplicates'])
print('Packets that showed up via RTX after RTX recovery:', stats['rtx_duplicate_recoveries'])

print('\nTotal frames:', len(final_frames))

def display_percentage(label, stat, divisor):
    print('{:s}: {:d} ({:.1f}%)'.format(label, stat, stat * 100.0 / divisor))

# this is really frames where one or more subsequent frames was assembled first and so probably stalled
display_percentage('"Late" frames', len(late_frames), len(final_frames))
display_percentage('Stalled frames', total_stall_count, len(final_frames))

# TODO: determine number of frames that were perfectly good, needed FEC, needed RTX
intact_frames = 0
fec_recovered_frames = 0
rtx_recovered_frames = 0
mixed_recovered_frames = 0
unrecovered_frames = 0

type_counts = {}

rtx_recovery_count = 0
rtx_recovery_total = 0
rtx_recovery_times = []
rtx_recovery_max = 0
rtx_over_100 = 0
rtx_over_150 = 0
rtx_over_200 = 0

recov_times = []

framenum = 0
for first, last, timestamp in final_frames:
    framenum += 1
    lost = 0
    fec = 0
    rtx = 0

    min_time = 10000
    max_time = 0

    for seq_num in range(first, last + 1):
        found = False
        for packet in save_packets[seq_num]:
            if time_matches_frame(packet.timestamp, timestamp):
                found = True
                break
        if not found:
            print(timestamp, seq_num)
            # this can happen on very late frames, so don't raise an exception for now
            raise Exception("packet not found that matches frame - increase match threshold?")
        if packet.was_missing:
            lost += 1
            if packet.fec:
                fec += 1
            elif packet.rtx:
                rtx += 1
                if packet.rtx_recovery > max_time:
                    max_time = packet.rtx_recovery
        if packet.timestamp < min_time:
            min_time = packet.timestamp
        if packet.timestamp > max_time:
            max_time = packet.timestamp

    recov_time = int((max_time - min_time) * 1000)
    if recov_time < 0:
        continue
#    print("Frame #{:d} completed in {:d} ms".format(framenum, recov_time))
    recov_times.append(recov_time)

    if rtx > 0:
        rtx_recovery_count += 1
        rtx_recovery_total += recov_time
        if recov_time > rtx_recovery_max:
            rtx_recovery_max = recov_time
        rtx_recovery_times.append(recov_time)
        if recov_time > 100:
            rtx_over_100 += 1
            if recov_time > 150:
                rtx_over_150 += 1
                if recov_time > 200:
                    rtx_over_200 += 1
    
    if lost > 0:
        if fec > 0 and rtx > 0:
            mixed_recovered_frames += 1
        elif fec > 0:
            fec_recovered_frames += 1
        elif rtx > 0:
            rtx_recovered_frames += 1
        else:
            unrecovered_frames += 1
    else:
        intact_frames += 1

    recov_type = '{:d}-{:d}-{:d}'.format(lost, fec, rtx)
    count = type_counts.get(recov_type, 0)
    type_counts[recov_type] = count + 1

rt_total = 0
rt_count = 0
rt_min = 3000
rt_max = 0
rt_under10 = 0
rt_under100 = 0
rt_under200 = 0
rt_over200 = 0

for rt in recov_times:
    rt_count += 1
    rt_total += rt
    if rt < rt_min:
        rt_min = rt
    if rt > rt_max:
        rt_max = rt
    if rt < 10:
        rt_under10 += 1
    elif rt < 100:
        rt_under100 += 1
    elif rt < 200:
        rt_under200 += 1
    else:
        rt_over200 += 1

rt_median = sorted(recov_times)[rt_count // 2]
print('Minimum frame completion time: {:d} ms'.format(rt_min))
print('Maximum frame completion time: {:d} ms'.format(rt_max))
print('Mean frame completion time: {:.1f} ms'.format(rt_total * 1.0 / rt_count))
print('Median frame completion time: {:d} ms'.format(rt_median))
print('Time under 10ms, 10-99, 100-199, 200+:', rt_under10, rt_under100, rt_under200, rt_over200)

display_percentage('\nIntact frames', intact_frames, len(final_frames))
display_percentage('Recovered by FEC alone', fec_recovered_frames, len(final_frames))
display_percentage('Recovered by RTX alone', rtx_recovered_frames, len(final_frames))
display_percentage('Recovered by both', mixed_recovered_frames, len(final_frames))
if unrecovered_frames > 0:
    print('Unrecovered frames:', unrecovered_frames)

print('\nMax RTX recovery time: {:d} ms'.format(rtx_recovery_max))
print('Average RTX recovery time: {:.1f} ms'.format(rtx_recovery_total * 1.0 / rtx_recovery_count))
print('RTX recovery times over 100, 150, 200 ms: {:d}, {:d}, {:d}'.format(rtx_over_100, rtx_over_150, rtx_over_200))

display_recovery_types = True
if display_recovery_types:
    print('\nFrame counts by recovery type (lost-fec-rtx):')
    count_to_type = {}
    for rtype in type_counts:
        count = type_counts[rtype]
        type_list = count_to_type.get(count, [])
        type_list.append(rtype)
        count_to_type[count] = type_list

    for count in sorted(count_to_type.keys()):
        type_list = count_to_type[count]
        percent = count * 100.0 / len(final_frames)
        each = '' if len(type_list) == 1 else ' each'
        pstr = ''
        if percent >= 0.05:
            pstr =  ' ({:.1f}%{:s})'.format(percent, each)
        print('{:d}: {:s}{:s}'.format(count, ', '.join(type_list), pstr))

