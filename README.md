Tools to assist in decision-making for the browser game [Trimps]
(https://trimps.github.io/).

Copyright © 2018-2019  Mikkosoft Productions
Licensed under GPLv3.


## Compiling

To compile the programs, simply type make.  No additional libraries are
required.


## Spire optimizer

The spire program can be used for optimizing spire tower defense layouts.  It
utilizes a genetic algorithm to search for the layout with best damage or
income.  In the simplest form, pass your current layout as a command line
argument and the program will try to find a layout that does more damage while
not requiring more runestones.  The behaviour can be further customized with
options.  Some of the more important are:

-f, --floors  
  Set the number of floors in the spire

-b, --budget  
  Set an upper limit of runestones to spend

-u, --upgrades  
  Set upgrade levels of all trap types at once.  The argument should be four
  numbers, one for each trap type.

-c, --core  
  Set stats of the spire core in use.  Core description must start with a tier
  name, followed by any number of mods separated by slashes.  Each mod is
  described by the trap or tower name, optionally followed by a colon and the
  value of the mod.  For example "epic/poison:40/lightning:30/condenser:12".

-i, --income  
  Optimize for runestone income instead of damage.  This is much slower so it
  may be prudent to first find a decent damage-optimized build and use that
  as a base for runestone optization.

--towers  
  Try to use as many towers as possible.  This may produce layouts that have
  inferior damage or income.  An optional argument can be used to specify the
  tower to maximize.  Prepending the tower name with a - will maximize other
  towers instead.

-n, --numeric-format  
  Output layouts in numeric format compatible with [swaq's TD calculator]
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
  that other users submit into the database.  Implies --online.

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

-r, --cross-rate  
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


## Perk optimizer

The perks program calculates optimal level for perks.  It has three required
command-line arguments: base population, target zone and helium budget.
Options can be used to further modify optimization goals:

--attack  
  Set the weight for trimp attack

--health  
  Set the weight for trimp health

--health  
  Set the weight for helium gain

--fluffy  
  Set the weight for fluffy exp gain

Each perk has an option which sets a base level for that perk.  The optimizer
may add further levels but will never remove levels.

--looting  
--toughness  
--power  
--toughness2  
--power2  
etc.

Other options are available to set auxiliary modifiers:

--breed-time  
  Set your target breed time in seconds

--equip-time  
  Set the number of seconds you're willing to spend on gearing up at the
  target zone

--heirloom-attack  
--heirloom-health  
--heirloom-crit-chance  
--heirloom-crit-damage  
--heirloom-miner  
  Set heirloom values

--large  
  Set value of the "large" daily challenge modifier (less housing)

--famine  
  Set value of the "famine" daily challenge modifier (less income)

--achievements  
  Set the damage bonus gained from achievements

--challenge2  
  Set the stat bonus gained from Challenge²
