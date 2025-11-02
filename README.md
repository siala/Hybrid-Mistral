
Description
-----------

Hybrid-Mistral is a hybrid CP/SAT solver designed to tackle Disjunctive Scheduling Problems. The SAT engine implements standard Conflict-Driven Clause Learning (CDCL) techniques such as Adaptive Branching (VSIDS), Two-Watched Literals, and 1-UIP Cuts. The latter features are built on top of Mistral-2.0. Propagators are augmented with explanation routines that are invoked only during conflict analysis.

Installation
--------------
To use the Scheduler module, first compile the code using 

```sh
 make scheduler 
```


That's all! Try it out with 


```sh
bin/scheduler data/scheduling/jsp/taillard/tai01.txt -type jsp
```

The general syntax of the command is the following 

```sh
bin/scheduler BENCHNAME -type [jla|jsp|osp] [-options]
```

Where BENCHNAME is the instance file location and "-type [jla|jsp|osp]" indicates its type. The instances are available in:


        * data/scheduling/jsp/taillard/ for JSP Taillard instances. The option -type should have the value jsp (default value)
        * data/scheduling/jla/Lawrence/ for JSP Lawrence instances. The option -type should have the value jla 
        * data/scheduling/osp/gueret-prins/ for OSP Gueret&Prins instances. The option -type should have the value osp 
        * data/scheduling/osp/taillard/ for OSP Taillard instances. The option -type should have the value osp 
        * data/scheduling/osp/hurley for OSP Brucker et.al instances. The option -type should have the value osp 

By default, a pure CP model is launched. To enable Clause Learning, the option "-fdlearning 2" should be specified. A large number of parameters can be used alongside "-fdlearning". I'm using mainly the following command line 

```sh
bin/scheduler BENCHNAME  -fdlearning 2 -semantic 1 -keeplearning 1   -orderedexploration 1 -reduce 1 -forgetall 0   -vsids 1
```


Parameters
----------

Several parameters can be used. These are probably the most important ones used with Clause Learning (I'll update the list later) : 

* -fdlearning : Used to switch on Clause Learning. Possible values are: 
		
        * 0 : No Learning
		* 1 : A naive Learning based on the decisions taken
		* 2 : CDCL 
        
        Default value : 0

* -semantic : [binary 1/0] enable Semantic Learning
 
		Default value : 0

* -vsids : [binary 1/0] enable VSIDS
 
		Default value : 0

* -lazygeneration : [binary 1/0] lazily generate bound literals needed in the learnt clause 
        
        Default value : 0
        

* -orderedexploration : [binary 1/0] Explore the Conflict Graph by following the order of propagation 

        Default value : 0

* -reduce : reduce the Learnt Clause. Possible values {0,1}
		
        Default value : 0

* -forgetsize : [positive integer] Used to learn only the nogoods having a size < to this parameter. 
		
        Default value : 0 (no check is performed)

* -forgetbackjump [positive integer] Used to learn only the nogoods leading to a difference of backtrack levels at least equal to this parameter
		
        Default value : 0 (no check is performed)

* -keeplearning [binary 1/0] Used to Enable Learning with Branch and Bound (B&B)
		
        Default value : 0

* -nogoodweight [binary 1/0] Use nogood-based weight within the WeightedDegree Heuristic
		
        Default value : 0


* -simulaterestart {0,1,2} : Used to simulate restart whenever a new bound is found with B&B. Value 2 will initialise the heuristic weights.

        Default value : 0 (no check is performed)
        
        
* -weighthistory : Use the weight history between dichotomy steps and/or B&B. 
 
        * Value  =0 : No history used
        * Value  =1 : The history is updated for each SAT result in the dichotomy but used only to update the weights before starting B&B
        * Value  =2 : The history is updated for each SAT result in the dichotomy but used only to update the weights before each dichotomy step
        * Value  =3 : The history is updated for each SAT result in the dichotomy and used to update the weights before each dichotomy step and before starting B&B
		
        Default value : 0 (no checking is performed)

* -restart : restart policy used with dichotomy : Possible values are :
		
        * "geom" for Geometric Restart 
		* "luby" for Luby Restart 
		
        Default value : "geom"

* -bandbrestart : restart policy used with B&B : Possible values are :
		
        * "geom" for Geometric Restart 
    	* "luby" for Luby Restart 
		
        Default value : "geom"


* -fixedForget : [integer] a fixed number of failure to reach before calling the clause deletion routine.
		
        Default value : 10000

* -fixedlimitSize : [integer] used to trigger clause deletion. If the size of the clause database is at least equal to this parameter then we start forgetting clauses. 
		
        Default value : 2000

* -fixedLearntSize : [integer] A fixed size of the clause database after the reduction.
		
        Default value : 2000


* -maxnogoodsize : [integer] Clauses that have a size at most equal to this parameters will be kept when performing forget. 


* -probforget : [positive integer bounded by 100] The probability that a large clause (w.r.t maxnogoodsize) will be forgotten. By default equal to 100. 



* -forgetfulness : [double] the % of forgetfulness of clauses.

        Default value : 0.0


* -bjmforgetfulness : [double] the % of backjump level forgetfulness of clauses. Very similar to -forgetbackjump, however, with percentage. 
		
        Default value : 0.0 (i.e. no check is performed)

[Mistral-2.0]:https://github.com/ehebrard/Mistral-2.0/
[Taillard instances]:http://mistic.heig-vd.ch/taillard/problemes.dir/ordonnancement.dir/ordonnancement.html 
