 

#include "mistral_scheduler.hpp"


using namespace Mistral;


int main( int argc, char** argv )
{

  ParameterList params(argc, argv);
  usrand(params.Seed);

  StatisticList stats;
  stats.start();

  Instance jsp(params);
  
  //std::cout << std::endl;
  //jsp.print(std::cout);
  jsp.printStats(std::cout);


  SchedulingSolver *solver;
  if(params.Objective == "makespan") {
    std::cout << " c Minimising Makespan" << std::endl;
    if(params.Type == "now") solver = new No_wait_Model(jsp, &params, -1, 0);
    else if(params.Type == "now2") {
      //params.Type = "now";
      solver = new No_wait_Model(jsp, &params, -1, 1);
    }
    else solver = new C_max_Model(&jsp, &params, &stats);
  } else if(params.Objective == "tardiness") {
    std::cout << "c Minimising Tardiness" << std::endl;
    solver = new L_sum_Model(jsp, &params, -1);
  } // else if(params.Objective == "depth") {
  //   std::cout << "c Minimising Depth" << std::endl;
  //   solver = new Depth_Model(jsp, &params, jsp.getMakespanUpperBound());
  // } else if(params.Objective == "weight") {
  //   std::cout << "c Minimising Weight" << std::endl;
  //   solver = new DTP_Model(jsp, &params, 1000);
  // }
  else {
    std::cout << "c unknown objective, exiting" << std::endl;
    exit(1);
  }

  // SchedulingSolver solver(model, &params, &stats);
  usrand(params.Seed);
  params.print(std::cout);

  solver->consolidate();

  //std::cout << solver << std::endl;

  // solver.print(std::cout);
  // //exit(1);

  // params.print(std::cout);  
  // model->printStats(std::cout);  
  // stats.print(std::cout, "INIT");  


  // //if
  // //solver.jtl_presolve();

  // //exit(1);

  // if(solver.status == UNKNOWN) 

  std::cout << "c Model : " << solver << std::endl;

  int old_distance = solver->get_ub() -  solver->get_lb();
  //solver->dichotomic_search(0);

  //if (params.lbCutoff){
  //  std::cout << " c Improving LB " << std::endl;
  //  old_distance = stats.upper_bound - stats.lower_bound;
  //  solver->lb_C_max = (stats.lower_bound);
  //  solver->ub_C_max = (stats.upper_bound);
  if (params.repeatdicho)
	  solver->dichotomic_search();
  else
	  solver->dichotomic_search(params.lbCutoff);

  //}

  //TODO add visited_objective
  while (params.repeatdicho--){

	  if(!stats.solved() && (old_distance > (stats.upper_bound - stats.lower_bound))) {
		  std::cout << " c repeat new dicho " << params.repeatdicho +1<< std::endl;
		  old_distance = stats.upper_bound - stats.lower_bound;
		  solver->lb_C_max = (stats.lower_bound);
		  solver->ub_C_max = (stats.upper_bound);
		  solver->dichotomic_search(params.lbCutoff);
	  }
	  else{
		  break;
	  }
  }




  // else if( solver.status == SAT ) {
  //   std::cout << "c Solved while building!" << std::endl;
  //   exit(1);
    
  // } else if( solver.status == UNSAT ) {
  //   std::cout << "c Found inconsistent while building!" << std::endl;
  //   exit(1);

  // }
      

  // stats.print(std::cout, "DS");

  if(!stats.solved()) {
	  //TODO we should reload a new model without ExplainedConstraints?

    if(params.Algorithm == "bnb")
      solver->branch_and_bound();
    //else if(params.Algorithm == "lns")
    //solver.large_neighborhood_search();
  }

  stats.print(std::cout, "");  
//  std::cout << "s " << (stats.num_solutions ? "SATISFIABLE" : "UNSATISFIABLE")
  //MOVE TO Optimality
  //Here SATISFIABLE means optimal and UNKNOWN means not optimal --> I added this only for parsing
  //std::cout << " s " << ((stats.lower_bound == stats.upper_bound) ? "SATISFIABLE" : "UNKNOWN")
  std::cout << " s " << ((stats.lower_bound == stats.upper_bound) ? "SATISFIABLE" : "SATISFIABLE")
     	    << "\n v 00" << std::endl;



#ifdef _PROFILING
  //solver->statistics.print_profile(std::cout);
  std::cout << solver->statistics.total_propag_time << std::endl;
#endif

  // delete mod
  // delete model;
  delete solver;
}
  




