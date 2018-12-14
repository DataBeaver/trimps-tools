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
  Set upgrade levels of all trap types at once.  The argument should be four
  numbers, one for each trap type.

-n, --numeric-format
  Output layouts in numeric format compatible with swaq's TD calculator
  (http://swaqvalley.com/td_calc/)

You can also specify a starting layout on the command line.  Budget, floors
and upgrades are deduced from the layout if not explicitly specified.

More advanced options can be used to tweak the performance of the program or
the genetic algorithm:

-w, --workers
  Set the number of worker threads to use

-l, --loops
  Set the number of new layouts generated per cycle.  Smaller values may
  accelerate evolution but also cause higher synchronization overhead.

-p, --pools
  Set the number of population pools to run in parallel.  A higher number of
  pools enhances diversity but slows down evolution as each pool gets less
  time.

-s, --pool-size
  Sets the maximum size of each pool

-c, --cross-rate
  Sets the probability of crossing two layouts instead of mutating a single
  layout.  Expressed as a number out of 1000.

-f, --foreign-rate
  Sets the probability of picking the second layout for a cross from a random
  pool instead of the same as the first.  Expressed as a number out of 1000.

--heterogeneous
  Use a heterogeneous pool configuration.  This can help if the properties of
  the upgrade configuration cause evolution to get stuck at a local optimum.

Finally, a few options are mostly for debugging purposes:

-g, --debug-layout
  Print detailed information of an enemy's progress through the layout

--show-pools
  Continuously show the top layouts in each population pool while running
