Continuous extrude multiple shells
==================================

Little useless aesthetic/artwork kinda stuff.

Printing multiple shells within one print usually means to move the print-head
between each layer. That means that there are ugly seams between layers.

This program generates GCode so that multiple continuous shells can be
printed on the same printbed, consecutively.

Right now with very simplistic gantry avoidance: print diagonally.

    Usage: ./multi-shell-extrude -t <template> [-p <height-per-rotation-mm>]
    Template describes shape. The letters in that string describe the
    screw depth for a full turn.
    A template 'AAZZZAAZZZAAZZZ' is a screw with three parallel threads,
    with 'inner parts' (the one with the lower letter 'A') being 2/3
    the width of the outer parts. 'AAZZZ' would have one thread per turn.
    The string-length represents a full turn, so 'AAZZZZZZZZ' would
    have one narrow thread.
    The range of letters the depth. Try 'AAZZMMZZZZ'.
    Required parameter: -h <height>
             -t <template>     : template string, described above
             -h <height>       : Total height to be printed
             -n <number-of-screws> : number of screws to be printed
             -r <radius>       : radius of the smallest screw
             -R <radius-increment> : increment between screws
             -d <thread-depth> : depth of thread (default: radius/5)
             -l <layer-height> : Height of each layer
             -f <feed-rate>    : maximum, in mm/s
             -T <layer-time>   : min time per layer; dynamically influences -f
             -p <pitch>        : how many mm height a full screw-turn takes

Output is on stdout.

Each shell is extruded separately in a single spiral ('vase'-like) run, so that
there is no seam between layers. Multiple shells can be printed on the same bed,
so to not interfere, they are printed diagonally.

![Print diagonally][print]

The result are shells that can be screwed into each other

![Result][result]

Commandline here:
`./multi-shell-extrude -l 0.12 -d 2 -r 10 -R 1.5 -T 4 -n 4 -h 60`

TODO
====
The diagonal collision avoidance is a bit simplistic and might not be enough
for printers with small build volume. Model the print-head and gantry and configure
per command-line flags to make a more compact staggered print.

[print]: https://github.com/hzeller/multi-shell-extrude/raw/master/img/print.jpg
[result]: https://github.com/hzeller/multi-shell-extrude/raw/master/img/result.jpg
