Continuous extrude multiple shells
==================================

Inspiration
-----------
Little aesthetic/artwork kinda stuff. Nice to play with, but don't expect it
to be useful :)
Inspired by shells I have seen
created by [Chris K. Palmer](http://shadowfolds.com/):
<a href="http://shadowfolds.com/?p=54"><img src="https://github.com/hzeller/gcode-multi-shell-extrude/raw/master/img/chris-palmer-shells.jpg"></a>
<div align="right">Nested screws by Chris K. Palmer.</div>

Code
----
Printing multiple shells within one print usually means to move the print-head
between each layer; moving the print-head creates visible seams between layers.

This program generates GCode directly (no extra slicer needed) in a way that
multiple continuous shells are printed on the same printbed in sequence.

Compile it first

    $ make

Now you can use it; here a little synopsis:

     Usage: ./multi-shell-extrude -h <height> [<options>]
     Required parameter: -h <height>
     
      [ Screw from a string template]
         -t <template>     : template string, described above
                             (default "AABBBAABBBAABBB")
                           The following options -d and -w work for this:
         -d <thread-depth> : depth of thread (default: radius/5)
         -w <twist>        : tWist ratio of angle per radius fraction
                             (good range: -0.3...0.3)

      [ Screw from a polygon data file ]
         -D <data>         : data file with polygon. Lines with x y pairs.

      [ General parameters ]
         -h <height>       : Total height to be printed
         -s <initial-size> : Polygon sizing parameter.
                             Means radius if from -t, factor for -D.
         -u <pump>         : Pump up polygon as if the center was not a
                             dot but a circle of <pump> radius
         -n <number-of-screws> : number of screws to be printed
         -R <radius-increment> : increment between screws, the clearance.
         -l <layer-height> : Height of each layer.
         -f <feed-rate>    : maximum, in mm/s
         -T <layer-time>   : min time per layer; dynamically influences -f
         -p <pitch>        : how many mm height a full screw-turn takes
         -L <x,y>          : x/y size limit of your printbed.
         -o <dx,dy>        : dx/dy offset per print. Clearance needed from
                             hotend-tip to left and front essentially.
     
      [ Output options ]
         -P                : PostScript output instead of GCode output
         -m                : For Postscript: show nested (Matryoshka doll style)

(TODO: make these long-options. The short-options are not really descriptive)

Output (GCode or PostScript) is on stdout.

Make sure to give the machine limits of your particular machine with the `-L` and
`-o` option to get the most screws on your bed.

Each shell is extruded separately in a single spiral ('vase'-like) run, so that
there is no seam between layers. Multiple shells can be printed on the same bed:
each vase is printed to its full height, then the next one is printed next to
it. To avoid physical collisions, they are printed diagonally so that the
printhead does not touch the already printed one (Use the `-o` option to configure
the needed clearance).

![Print diagonally][print]
(Type-A Machine Series 1 2014)

The result are shells that can be screwed into each other

![Result][result]

This result was created with this commandline:

    ./multi-shell-extrude -l 0.12 -d 2 -r 10 -R 1.5 -T 4 -n 4 -h 60 -L 305,305 -o 50,50 > out.gcode

Printed twice with different filaments.

![Printrbot][printrbot]
(Printrbot Simple Metal)

`./multi-shell-extrude -l 0.12 -d 2 -r 16 -R 1.5 -T 4 -n 2 -h 60 -L150,150 -o50,50 > /tmp/screw-printrbot.gcode`

Describing a screw with a template
----------------------------------

(TODO: describe better)

Template (flag `-t`) describes the shape. The letters in that string
describe the screw depth for a full turn.
A template `AAZZZAAZZZAAZZZ` is a screw with three parallel threads,
with 'inner parts' (the one with the lower letter 'A') being 2/3
the width of the outer parts. `AAZZZ` would have one thread per turn.
The string-length represents a full turn, so `AAZZZZZZZZ` would
have one narrow thread.
The range of letters (here A..Z) is linearly mapped to the thread
depth.
Try 'AAZZMMZZZZ'. If you only use two letters, then 'AABBBAABBB'
is equivalent to 'AAZZZAAZZZ'

Twist
-----

The experimental `-w` parameter allows to give things a little twist:

     ./multi-shell-extrude -l 0.12 -p 120 -d 12 -r 10 -R 2 -T 4 -n 4 -h 60 -L 305,305 -o 50,50 -w 0.2 -t ABABABABAB > twist.gcode

PostScript output
-----------------

While playing with the options, it is nicer to first look at how it would look
on the printbed. So there is the option `-P` to output postscript instead of
GCode. If you use a PostScript viewer such as `okular` that re-loads the content
whenever the file changes, you can play with the options and see how things
change. The first three layers of the spiral are shown, which gives an
indication if the filament would stick.

     $ ./multi-shell-extrude -h 5 -P > output.ps
     $ okular output.ps &    # start postscript viewer
     # now play around with the options and watch the changes
     $ ./multi-shell-extrude -h 5 -d 10 -w 0.3 -t BAAAABAAAABAAAA -P > output.ps


TODO
----
The diagonal collision avoidance is a bit simplistic and wasting space. Might
not be enough for printers with small build volume. Model the print-head and
gantry and configuration to make a more compact staggered print.

Have Fun!
---------
![Multiple Screws from different prints][multiple-prints]


[print]: https://github.com/hzeller/gcode-multi-shell-extrude/raw/master/img/print.jpg
[printrbot]: https://github.com/hzeller/gcode-multi-shell-extrude/raw/master/img/printrbot.jpg
[result]: https://github.com/hzeller/gcode-multi-shell-extrude/raw/master/img/result.jpg
[multiple-prints]: https://github.com/hzeller/gcode-multi-shell-extrude/raw/master/img/multiscrew.jpg
