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

Now you can use it; here a little synopsis that you get if you invoke the program
without parameters.

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
         -p <pitch>        : how many mm height a full screw-turn takes.
                             negative for left screw. 0 for straight hull
         -L <x,y>          : x/y size limit of your printbed.
         -o <dx,dy>        : dx/dy offset per print. Clearance needed from
                             hotend-tip to left and front essentially.

      [ Output options ]
         -P                : PostScript output instead of GCode output
         -m                : For Postscript: show nested (Matryoshka doll style)

(TODO: make these long-options. The short-options are not really descriptive)

Output (GCode or PostScript) is on stdout.

See sample invocations below in the Gallery.

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

The result are shells that can be screwed into each other.

Screw description
-----------------

### Describing a screw with a template

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
Try `AAZZMMZZZZ`. If you only use two letters, then `AABBBAABBB`
is equivalent to `AAZZZAAZZZ`

#### Twist


The experimental `-w` parameter allows to give things a little twist:

      ./multi-shell-extrude -p 120 -d 12 -h 60 -w 0.2 -t ABABABABAB > twist.gcode

Note, we are giving a relatively high pitch value to manage the overlaps
between layers - see below in PostScript output an example.

### Reading Polygon from File

Alternatively, you can read an arbitrary polygon from a file. The vertices need
to be given counterclock wise. The rotation of the resulting screw will be
around the origin.
As an example, see `sample/hilbert.poly`

PostScript output
-----------------

While playing with the options, it is nicer to first look at how it would look
on the printbed. So there is the option `-P` to output postscript instead of
GCode. If you use a PostScript viewer such as `okular` that re-loads the content
whenever the file changes, you can play with the options and see how things
change. The first three layers of the spiral are shown, which gives an
indication if the filament would stick - if the rotation is too quick, you see
separate overlapping lines. The thickness of the shell is roughly represented
by the thickness of the line in the postscript output.

     $ ./multi-shell-extrude -h 5 -P > out.ps
     $ okular out.ps &    # start postscript viewer
     # now play around with the options and watch the changes. In the following
     # example, we set the pitch too steep, so we see overlaps
     $ ./multi-shell-extrude -n 1 -p 10 -h 5 -d 10 -w 0.3 -t BAAAABAAAABAAAA -P > out.ps

We deliberately chose a very steep pitch here (`-p`; here full turn in 10mm).
The layers of the 3D print now already miss the corresponding previous
layer -- they are not overlapping sufficiently anymore. We can see this in the
following PostScript output, so we don't mess up a 3D print.

![Steep pitch][steep-pitch]

To fix, either increase pitch `-p` (number of mm height for a full turn) or
decrease layer-height `-l`.

The usual view displays exactly the layout on the print-bed with all screws
spread out.

![Postscript bed view][postscript-bed-layout]

If you want to see how the screws nest, add the `-m` parameter
(like [Matryoshka][matryoshka-reference], the nested doll),
then they are all shown nested into each other

     ./multi-shell-extrude -n 5 -h 10 -p 180 -s 3.5 -D sample/hilbert.poly -P -m > out.ps

![Matryoshka hilbert][matryoshka-hilbert]

     ./multi-shell-extrude -n 5 -h 10 -p 180 -s 10 -D sample/snowflake.poly -P -m > out.ps

![Matryoshka Snowflake][matryoshka-snowflake]

Here a regular screw, 5 nested into each other:

     ./multi-shell-extrude -n 5 -h 10 -p 180 -s 10 -d 5 -t aaabaaabaaab -P -m > out.ps

![Matryoshka screw][matryoshka-screw]

.. how about 20 nested screws ?

    ./multi-shell-extrude -n 20 -h 10 -p 180 -s 5 -d 5 -t aaabaaabaaab -P -m > out.ps

![Many screws][many-screws]

Gallery and Examples
--------------------

### Simple screw

![Result][result]

This result was created with this commandline:

    ./multi-shell-extrude -l 0.12 -d 2 -s 10 -R 1.5 -n 4 -h 60 -L 305,305 -o 50,50 > out.gcode

Printed twice with different filaments.

![Printrbot][printrbot]
(Printrbot Simple Metal)

`./multi-shell-extrude -l 0.12 -d 2 -s 16 -R 1.5 -n 2 -h 60 -L150,150 -o50,50 > /tmp/screw-printrbot.gcode`

### Polygon from data file

Here, we generate two different colors in two prints. We want to print the
hilbert example from the `sample/` directory. The shells should be 1.2mm apart.
We want them alternating in color, so we do to prints with different colors
with each 2.4mm apart. For that, we give both the shell increment value
`-R 2.4`, but with different start values with the `-i` option.
The blue print starts with `-i 0`, so no initial offset. The
orange print starts with `-i -1.2` (yes you can give negative values, then a polygon is constructed that fits on the _inside_).

Now orange prints the offsets [ -1.2mm, 1.2mm, 3.6mm ], while blue
prints [ 0mm, 2.4mm, 4.8mm ].

In general, to spread over multiple prints, you can use the initial shell value
`-i` together with the radius increment `-R` to generate the right offsets.

The resulting screws nest together nicely with 1.2mm distance:

     ./multi-shell-extrude -n 3 -i -1.2 -R 2.4 -h 60 -p 180 -L220,220 -s 3.5 -D sample/hilbert.poly > /tmp/orange.gcode
     ./multi-shell-extrude -n 3 -i  0   -R 2.4 -h 60 -p 180 -L220,220 -s 3.5 -D sample/hilbert.poly > /tmp/blue.gcode

![Print Plan hilbert][orange-blue]
![Hilbert Screws][hilbert-screw]
![Shell-view][hilbert-shells]

TODO
----
The diagonal collision avoidance is a bit simplistic and wasting space. Might
not be enough for printers with small build volume. Model the print-head and
gantry and configuration to make a more compact staggered print.

Have Fun!
---------
![Multiple Screws from different prints][multiple-prints]


[print]: ./img/print.jpg
[printrbot]: ./img/printrbot.jpg
[result]: ./img/result.jpg
[hilbert-screw]: ./img/hilbert-screw.jpg
[hilbert-shells]: ./img/hilbert-shells.jpg
[multiple-prints]: ./img/multiscrew.jpg
[postscript-bed-layout]: ./img/postscript-bed-layout.png
[matryoshka-hilbert]: ./img/matryoshka-hilbert.png
[matryoshka-snowflake]: ./img/matryoshka-snowflake.png
[matryoshka-screw]: ./img/matryoshka-screw.png
[many-screws]: ./img/many-screws.png
[steep-pitch]: ./img/steep-pitch.png
[orange-blue]: ./img/orange-blue.png
[matryoshka-reference]: http://en.wikipedia.org/wiki/Matryoshka_doll
