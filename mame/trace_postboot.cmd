# Wait for the post-boot state, then record 10 ms of the main CPU.
focus maincpu
gtime #10000
trace work/mame/traces/postboot-10ms.trace,maincpu
gtime #10
trace off,maincpu
traceflush
quit
