Continuous extrude multipole shells
===================================

Printing multiple shells within one print usually means to move the print-head
between each layer. That means that there are ugly seams between layers.

This probram generates GCode so that multiple continuous shells can be
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
             -l <layer-height> : Height of each layer
             -f <feed-rate>    : in mm/s
             -p <pitch>        : how many mm height a full screw-turn takes

Output is on stdout.
