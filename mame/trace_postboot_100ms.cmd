# Approximately six frames: enough to observe the loop repeating.
focus maincpu
gtime #10000
trace work/mame/traces/postboot-100ms.trace,maincpu
gtime #100
trace off,maincpu
traceflush
quit
