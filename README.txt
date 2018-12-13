Tools to assist in decision-making for the browser game Trimps
(https://trimps.github.io/).

To compile, simply type make.  No additional libraries are required.

The spire program can be used for optimizing spire tower defense layouts.  It
utilizes a genetic algorithm to search for the layout with best damage.  Some
of the more important options are:

-f, --floors
  Set the number of floors in the spire

-b, --budget
  Set an upper limit of runestones to spend

--fire, --frost, --poison, --lightning
  Set upgrade levels of each trap type

-u, --upgrades
  Set upgrade levels of all trap types at once

-n, --numeric-format
  Output layouts in numeric format compatible with swaq's TD calculator
  (http://swaqvalley.com/td_calc/)

You can also specify a starting layout on the command line.
