Continuous extrude multiple shells
==================================

Little aesthetic/artwork kinda stuff. Nice to play with, but don't expect it
to be useful :)
Inspired by shells I have seen
created by [Chris K. Palmer](http://shadowfolds.com/).

Printing multiple shells within one print usually means to move the print-head
between each layer; moving the print-head creates visible seams between layers.

This program generates GCode directly (no extra slicer needed) in a way that
multiple continuous shells are printed on the same printbed in sequence.

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
             -L <x,y>          : x/y size limit of your printbed.
             -o <dx,dy>        : dx/dy offset per print. Clearance needed from
                                 hotend-tip to left and front essentially.

GCode-output is on stdout.

Make sure to give the machine limits of your particular machine with the `-L` and
`-o` option to get the most screws on your bed.

Each shell is extruded separately in a single spiral ('vase'-like) run, so that
there is no seam between layers. Multiple shells can be printed on the same bed:
each vase is printed to its full height, then the next one is printed next to
it. To avoid physical collisions, they are printed diagonally so that the
printhead does not touch the already printed one (Use the `-o` option to configure
the needed clearance).

![Print diagonally][print]

The result are shells that can be screwed into each other

![Result][result]

This result was created with this commandline:

    ./multi-shell-extrude -l 0.12 -d 2 -r 10 -R 1.5 -T 4 -n 4 -h 60 -L 305,305 -o 50,50 > out.gcode

Printed twice with different filaments.

TODO
====
The diagonal collision avoidance is a bit simplistic and might not be enough
for printers with small build volume. Model the print-head and gantry and configure
per command-line flags to make a more compact staggered print.

[print]: https://github.com/hzeller/gcode-multi-shell-extrude/raw/master/img/print.jpg
[result]: https://github.com/hzeller/gcode-multi-shell-extrude/raw/master/img/result.jpg
