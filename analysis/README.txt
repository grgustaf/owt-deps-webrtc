run_ga.sh
---------
Bash script, I run this from git bash on Windows.
This one's trivial but just helps me name my log files consistently.
Note: peerserver URL is hardcoded in here

$ ./run_ga.sh NAME LATENCY LOSSRATE
E.g. for latency 30ms and 5% loss I do this:
$ ./run_ga.sh more_nacks 30 5


analyze.sh
----------
Another Bash script, this one looks at the logfile and reports a few statistics.

It's expecting all the output from my detailed-logging-latest branch of webrtc, plus
the GetStats() output from your patch to be dumped at least once.

The GetStats output is probably coming from a different thread because sometimes
it gets interleaved badly with the "detailed logging" output and can confuse my
scripts. I've just been manually patching up the line or two where that happens.

$ ./analyze.sh LOGFILE
E.g.
$ ./analyze.sh more_nacks-30-5.log


frames.py
---------
A Python 3 script I also run from git bash.
This looks at most of the logged information, all the video, FEC, and RTX packets,
and the assembled frames, and reconstructs which packets went with which frame,
and what happened to them - arrived, missing, recovered by FEC, RTX, etc.

It finds frames that were completed late and held up subsequent frames, and
dumps detailed logging info for them, like I showed you in an email the other
day.

After lots of this dumping, displays some interesting stats at the very end.

If it gives an exception, there is probably a log line that got merged with another
in the output and needs manual intervention (usually just inserting a newline). If you
print the timeline and seq_num at the Exception, then search for that seq_num in the
log file, around that time you should fine a merged line that needs to be split
up.

$ python frames.py LOGFILE
