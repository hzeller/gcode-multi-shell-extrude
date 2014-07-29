Continuous extrude multipole shells

Printing multiple shells within one print usually means to move the print-head
between each layer. That means that there are ugly seams between layers.

This probram generates GCode so that multiple continuous shells can be
printed on the same printbed. Since that means that after one is finished
we need to go back to Z=0, this takes the gantry/printhead movement as
exclude area into account.
