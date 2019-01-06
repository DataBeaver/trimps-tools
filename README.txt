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

-u, --upgrades
  Set upgrade levels of all trap types at once.  The argument should be four
  numbers, one for each trap type.

-i, --income
  Optimize for runestone income instead of damage.  This is much slower so it
  may be prudent to first find a decent damage-optimized build and use that
  as a base for runestone optization.

--towers
  Try to use as many towers as possible.  This may produce layouts that have
  inferior damage or income.

-n, --numeric-format
  Output layouts in numeric format compatible with swaq's TD calculator
  (http://swaqvalley.com/td_calc/)

--fancy
  Produces fancy pseudographical output.  For best result use a 256-color
  xterm or compatible terminal.

--online
  Use the online layout database.  On startup the database is queried for the
  best known layout.  As better layouts are found they are submitted to the
  database.

--live
  Perform database query in live mode and automatically obtain improvements
  that other users submit into the database.

You can also specify a starting layout on the command line.  Budget, floors
and upgrades are deduced from the layout if not explicitly specified.

More advanced options can be used to tweak the performance of the program or
the genetic algorithm:

-t, --preset
  Choose a predefined set of options.  Available presets are single, basic,
  diverse and advanced.  This option is relatively safe to use even if you
  have no clue what the following ones are for.

-e, --exact
  Use exact calculation mode when optimizing for damage.  This reduces
  performance but guarantees that damage is calculated correctly.  Income
  optimization always uses exact mode.

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

-o, --foreign-rate
  Sets the probability of picking the second layout for a cross from a random
  pool instead of the same as the first.  Expressed as a number out of 1000.

--heterogeneous
  Use a heterogeneous pool configuration.  This can help if the properties of
  the upgrade configuration cause evolution to get stuck at a local optimum.

--prune-interval
  Set the number of iterations before pruning the worst performing pool

--prune-limit
  Stop pruning when only this many pools are left

Finally, a few options are mostly for debugging purposes:

-g, --debug-layout
  Print detailed information of an enemy's progress through the layout

--show-pools
  Continuously show the top layouts in each population pool while running

--raw-values
  Print raw, full values of numbers.  These are more difficult to read but
  may be helpful in debugging suspected accuracy issues.
