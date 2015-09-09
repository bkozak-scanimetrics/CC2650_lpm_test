Builds an LPM Test Binary
=========================

Builds a Contiki application which turns off the radio, waits ten seconds
then puts the CC2650 into a permanent sleep mode so that the current
consumption can be measured.

Building
========

This project builds just like any other Contiki project. Just use
`make TARGET=<target>`. If, however, the contiki source tree does not reside at
`../../contiki_src/contiki` then you will need to use
`make CONTIKI=path/to/contiki/ TARGET=<target>` or edit the Makefile.
