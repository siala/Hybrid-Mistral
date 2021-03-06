 

#include "mistral_scheduler.hpp"
#include "mistral_sat.hpp"
#include <math.h>
#include <assert.h>


//For memory test
//#include <thread>         // std::this_thread::sleep_for
//#include <chrono>


using namespace Mistral;


//#define INFTY 0xffffff
#define BIG 0xffff
//#define _DEBUG_SCHEDULER true
#define DICHO 0
#define BNB   1
#define LNS   2


StatisticList::StatisticList() {
  best_solution_index     = 0;
  branch_and_bound_index  = 0;

  lower_bound             = INFTY;
  upper_bound             = 0;

  num_nogoods             = 0;
  avg_nogood_size         = 0;
  num_solutions           = 0;
  
  avg_distance            = 0;
  min_distance            = INFTY;
  max_distance            = 0;

  normalized_objective    = 1.0;

  real_start_time         = 0.0;
}

StatisticList::~StatisticList() {}

void StatisticList::start() {
  real_start_time = get_run_time();
}

// void StatisticList::stop() {
//   real_time = (get_run_time() - real_time);
// }

bool StatisticList::solved() {
  return (lower_bound == upper_bound);
}

double StatisticList::get_total_time() {
  double total_time  = 0.0;
  for(unsigned int i=0; i<time.size(); ++i)
    total_time += time[i];
  return total_time;
}

double StatisticList::get_lowerbound_time() {
  double total_time  = 0.0;
  for(unsigned int i=0; i<time.size(); ++i)
    if(outcome[i] == UNSAT)
      total_time += time[i];
  return total_time;
}

void StatisticList::add_info(const int objective, int tp) {

  //bool update_ub = true;
  DBG("Update statistics%s\n", "");

  double runtime = (solver->statistics.end_time - solver->statistics.start_time);
  //std::cout << "ADDINFO RUNTIME=" << runtime << std::endl;

  time.push_back(runtime);
  soltime.push_back(runtime);
  nodes.push_back(solver->statistics.num_nodes);
  backtracks.push_back(solver->statistics.num_backtracks);
  fails.push_back(solver->statistics.num_failures);
  propags.push_back(solver->statistics.num_propagations);
  var_relaxed.push_back(solver->statistics.generated_then_relaxed);
  var_reposted.push_back(solver->statistics.reposted_generated);
  avg_clauses_size.push_back(solver->statistics.avg_learned_size);
  types.push_back(tp);

  //std::cout << outcome2str(solver->statistics.outcome) << std::endl;

  outcome.push_back(solver->statistics.outcome);


  

  if(tp == BNB) {
    if(outcome.back() == OPT) {
      lower_bound = upper_bound = objective;
      best_solution_index = outcome.size()-1;
    } else if(outcome.back() == UNSAT) {
      lower_bound = objective;
      if (lower_bound<upper_bound)
    	  lower_bound++;
    } else {
      //upper_bound = objective;
      if((outcome.back() == SAT) || upper_bound> objective ){
     // ++num_solutions;
      best_solution_index = outcome.size()-1;
      upper_bound = objective;
      }
    }
  } else {
    if(outcome.back() == SAT || outcome.back() == OPT) {
      ++num_solutions;
      best_solution_index = outcome.size()-1;
      upper_bound = objective;
      if(outcome.back() == OPT) lower_bound = objective;
    } else if((types.back() != LNS) && (outcome.back() == UNSAT)) {
      lower_bound = objective+1;
    }
  }

  //std::cout << outcome2str(outcome.back()) << " ==> [" << lower_bound << ".." << upper_bound << "]" << std::endl;

}

int StatisticList::get_avgsizeofclauses(){

	  int k, i=0, j=outcome.size();
	  long unsigned int total_fails         = 0;

	  long unsigned int  total_clauses_size       = 0;

	  for(k=i; k<j; ++k) {
	    total_fails        += fails[k];
	    total_clauses_size += (fails[k] *avg_clauses_size[k]) ;
	  }

	  return total_clauses_size/total_fails;


}

std::ostream& StatisticList::print(std::ostream& os, 
				   const char* prefix,
				   const int start, 
				   const int end) {

  int k, i=start, j=outcome.size();
  if(end >= 0) j=end;

  double total_time      = 0.0;
  double opt_time        = 0.0;
  double ub_time         = 0.0;
  double lb_time         = 0.0;
  double lost_time       = 0.0;
  double proof_time      = 0.0;
  double avg_cutoff_time = 0.0;
  long unsigned int total_nodes         = 0;
  long unsigned int total_backtracks    = 0;
  long unsigned int total_fails         = 0;
  long unsigned int total_propags       = 0;
  unsigned int total_var__relaxed         = 0;
  unsigned int total_var__reposted       = 0;
  long unsigned int  total_clauses_size       = 0;
  
  int nb_unknown = 0;
  for(k=i; k<j; ++k) {
//     if(k<=best_solution_index) {
//       if(outcome[k] != OPT)
// 	opt_time += time[k];
//       else
// 	opt_time += soltime[k];
//     }
    if(k==best_solution_index) opt_time += soltime[k];
    else opt_time += time[k];
    
    if(outcome[k] != OPT && outcome[k] != UNSAT) {
      lost_time += (time[k] - soltime[k]);
    }
    
    ub_time            += soltime[k];
    total_time         += time[k];
    total_nodes        += nodes[k];
    total_backtracks   += backtracks[k];
    total_fails        += fails[k];
    total_propags      += propags[k];
    total_var__relaxed       += var_relaxed[k];
    total_var__reposted       += var_reposted[k];
    total_clauses_size += (fails[k] *avg_clauses_size[k]) ;
    if(types[k]==DICHO && outcome[k] == UNKNOWN)
      {
	++nb_unknown;
	avg_cutoff_time += time[k];
      }

  }

  if(nb_unknown)
    avg_cutoff_time /= (double)nb_unknown;
  proof_time = (total_time - opt_time);
  lb_time = (total_time - ub_time - lost_time);

  if(lb_time < 0.00001) lb_time = 0.0;
  if(lost_time < 0.00001) lost_time = 0.0;
  if(proof_time < 0.00001) proof_time = 0.0;


  avg_distance = 0;
  min_distance = NOVAL;
  max_distance = 0;
  for(i=1; (unsigned int)i<solver->pool->size(); ++i) {
    double dist = (*(solver->pool))[i-1]->distance((*(solver->pool))[i]);
    avg_distance += dist;
    if(dist>max_distance) max_distance = dist;
    if(dist<min_distance) min_distance = dist;
  }
  avg_distance /= solver->pool->size();


  int plength = 0;
  while(prefix[plength] != '\0') ++plength;
  
  os << "\n c +==============[ statistics ]===============+" << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " LOWERBOUND "    << std::right << std::setw(19) << lower_bound << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " UPPERBOUND "    << std::right << std::setw(19) << upper_bound << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " OBJECTIVE "     << std::right << std::setw(19) << upper_bound << std::endl 
    << " d " << prefix << std::left << std::setw(25-plength)  << " NORMOBJECTIVE " << std::right << std::setw(19) << normalized_objective << std::endl 
    << " d " << prefix << std::left << std::setw(25-plength)  << " REALTIME "      << std::right << std::setw(19) << (get_run_time() - real_start_time) << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " RUNTIME "       << std::right << std::setw(19) << total_time << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " OPTTIME "       << std::right << std::setw(19) << opt_time << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " PROOFTIME "     << std::right << std::setw(19) << proof_time << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " UBTIME "        << std::right << std::setw(19) << ub_time << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " LBTIME "        << std::right << std::setw(19) << lb_time << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " LOSTTIME "      << std::right << std::setw(19) << lost_time << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " NODES "         << std::right << std::setw(19) << total_nodes << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " BACKTRACKS "    << std::right << std::setw(19) << total_backtracks << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " FAILS "         << std::right << std::setw(19) << total_fails << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " PROPAGS "       << std::right << std::setw(19) << total_propags << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " RELAXED "       << std::right << std::setw(19) << total_var__relaxed << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " REPOSTED "       << std::right << std::setw(19) <<  total_var__reposted << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " NOGOODS "       << std::right << std::setw(19) << num_nogoods << std::endl;
  //No longer used
  if(num_nogoods)
	    std::cout << " d " << prefix << std::left << std::setw(25-plength)  << " NOGOODSIZE "    << std::right << std::setw(19) << avg_nogood_size/(double)num_nogoods << std::endl;
  if (total_fails)
  std::cout << " d " << prefix << std::left << std::setw(25-plength)  << " NOGOODSIZE "    << std::right << std::setw(19) << total_clauses_size / total_fails << std::endl;
  else
	  std::cout << " d " << prefix << std::left << std::setw(25-plength)  << " NOGOODSIZE "    << std::right << std::setw(19) << 0 << std::endl;

  std::cout << " d " << prefix << std::left << std::setw(25-plength)  << " NODES/s "       << std::right << std::setw(19) ;
  if(total_time > 0)
    std::cout << (int)((double)total_nodes/total_time);
  else 
    std::cout << "N/A";
  std::cout << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " BACKTRACKS/s "  << std::right << std::setw(19) ;
  if(total_time > 0)
    std::cout << (int)((double)total_backtracks/total_time);
  else 
    std::cout << "N/A";
  std::cout << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " FAILS/s "       << std::right << std::setw(19) ;
  if(total_time > 0)
    std::cout << (int)((double)total_fails/total_time);
  else 
    std::cout << "N/A";
  std::cout << std::endl
     << " d " << prefix << std::left << std::setw(25-plength)  << " PROPAGS/s "     << std::right << std::setw(19) ;
  if(total_time > 0)
    std::cout << (int)((double)total_propags/total_time);
  else 
    std::cout << "N/A";
  std::cout 
    << std::endl
    //<< " d " << prefix << std::left << std::setw(25-plength)  << "RESTARTS "      << std::right << std::setw(19) << total_restarts << std::endl
    
    << " d " << prefix << std::left << std::setw(25-plength)  << " SOLUTIONS "     << std::right << std::setw(19) << num_solutions << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " AVGCUTOFF "     << std::right << std::setw(19) << avg_cutoff_time << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " AVGDISTANCE "   << std::right << std::setw(19) << avg_distance << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " MINDISTANCE "   << std::right << std::setw(19) << min_distance << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " MAXDISTANCE "   << std::right << std::setw(19) << max_distance << std::endl
    << " d " << prefix << std::left << std::setw(25-plength)  << " OPTIMAL "       << std::right << std::setw(19) << (lower_bound == upper_bound) << std::endl
    << " c +==============[ statistics ]===============+" << std::endl
    << std::endl;

  return os;
}  


const char* ParameterList::int_ident[ParameterList::nia] = 
  {"-ub", "-lb", "-check", "-seed", "-cutoff",
   "-dichotomy", "-base", "-randomized", "-verbose", "-optimise",
   "-nogood", "-dyncutoff", "-nodes", "-hlimit", "-init",
   "-neighbor", "-initstep", "-fixtasks", "-order", "-ngdt" ,
   "-fdlearning" , "-forgetall" , "-reduce" ,  "-orderedexploration" , "-lazygeneration" ,
   "-semantic" , "-simplelearn" , "-maxnogoodsize" , "-boundedbydecision" , "-forgetsize" ,
   "-forgetbackjump" , "-hardkeep", "-hardforget" ,"-keepwhensize" , "-keepwhenbjm" ,
   "-keeplearning" , "-simulaterestart" , "-nogoodweight" , "-weighthistory" ,  "-fixedForget" ,
   "-fixedlimitSize" , "-fixedLearntSize" , "-probforget" ,"-forgetdecsize" , "-vsids",
   "-autoconfig" , "-autoconfigublimit" , "-autoconfiglblimit", "-limitresetpolicy" , "-taskweight" ,
   "-adaptsize" , "-adaptforget" , "-repeatdicho" , "-lbcutoff", "-sizeocc"
  };

const char* ParameterList::str_ident[ParameterList::nsa] = 
  {"-heuristic", "-restart", "-factor", "-decay", "-type", 
   "-value", "-dvalue", "-ivalue", "-objective", "-algo",
   "-presolve", "-bandbrestart" , "-forgetfulness", "-bjmforgetfulness" , "-bbforgetfulness"
  };


// ParameterList::ParameterList() {
// }

// ParameterList::ParameterList(const ParameterList& pl) {
//   initialise(pl);
// }

ParameterList::ParameterList(int length, char **commandline) {

  if( length < 2 )
    {
      std::cerr << "need a data file" << std::endl;
      exit( 0 );
    }

  
  data_file = commandline[1];

  int i=0;
  while(commandline[1][i] != '\0') ++i;
  while(commandline[1][i] != '/') --i;
  data_file_name = &(commandline[1][i+1]);

  get_command_line(ParameterList::int_ident,
		   int_param,
		   ParameterList::nia,
		   ParameterList::str_ident,
		   str_param,
		   ParameterList::nsa,
		   &commandline[1],length-1);

  if(strcmp(str_param[4],"nil")) Type = str_param[4];
  else {
    Type = "jsp";
    std::cout << " c Warning: no type specified, treating the data as Taillard's jsp" << std::endl;
  }

  UBinit      = -1;
  LBinit      = -1;
  Checked     = true;
  Seed        = 12345;
  Cutoff      = 300;
  NodeCutoff  = 0;
  NodeBase    = 20;


  lbCutoff  = 0;
  Dichotomy   = 128;
  Base        = 256;
  Randomized  = -1;
  Precision   = 100;
  Hlimit      = 20000;
  InitBound   = 1000;
  InitStep    = 1;
  Neighbor    = 2;

  Verbose     = 0;
  Optimise    = 3600;
  Rngd        = 2;

//  Policy    = "geom";
  Policy    = "geom";
  BandBPolicy    = "geom";
  Factor    = 1.3;
  Decay     = 0.0;
  Forgetfulness = 0.0;
  Forgetfulness_retated_to_backjump = 0.0;
  BBforgetfulness=0.0;
  Value     = "guided";
  DValue    = "guided";
  IValue    = "promise";
  Objective = "makespan";
  Algorithm = "bnb";
  Presolve  = "default";

  Heuristic = "none";
  PolicyRestart = GEOMETRIC;
  BandBPolicyRestart = GEOMETRIC;
  FixTasks  = 0;
  NgdType   = 2;
  OrderTasks = 1;



  orderedExploration = 0;
  lazy_generation= 0;
  semantic_learning = 0;

  simple_learn= 0;
  max_nogood_size =12;
  bounded_by_decision = 0;
  forget_relatedto_nogood_size = 0;
  forget_retatedto_backjump = 0;

  hard_keep =0;
  hard_forget =0;
  keep_when_size =0;
  keep_when_bjm =0;
  keeplearning_in_bb = 0;
  simulaterestart=0;
  nogood_based_weight = 0;

  weight_history=0;
  FD_learning=0;
  reduce_clauses =0;
  forgetall=1;

  fixedForget=5000;
  fixedlimitSize = 14000;
  fixedLearntSize = 2000;

  prob_forget=90;
  forgetdecsize=8;
  vsids=0;

  autoconfig =0;
  autoconfigublimit=150000;
  autoconfiglblimit=10000;
  limitresetpolicy=0;

  taskweight=0;

  adaptsize=0 ;
  adaptforget=0;

  repeatdicho=0;

  sizeocc=0;

  if(Type == "osp") {
    Objective = "makespan";
    if(Heuristic == "none")
      Heuristic = "osp-b";
  } else if(Type == "sds") {
    Objective = "makespan";
    if(Heuristic == "none")
      Heuristic = "osp-b";
  } else if(Type == "jtl") {
    Objective = "makespan";
    Presolve = "jtl";
    if(Heuristic == "none")
      Heuristic = "osp-t";
  } else if(Type == "now" || Type == "now2") {
    Objective = "makespan";
    Presolve = "default";
    if(Heuristic == "none")
      Heuristic = "osp-dw";
  } else if(Type == "jla") {
    Objective = "makespan";
    if(Heuristic == "none")
      Heuristic = "osp-t";
  } else if(Type == "jsp") {
    Objective = "makespan";
    if(Heuristic == "none")
      Heuristic = "osp-t";
  } else if(Type == "fsp") {
    Verbose = -1;
    OrderTasks = -1;
    Objective = "makespan";
    if(Heuristic == "none")
      Heuristic = "osp-t";
  } else if(Type == "jet") {
    Objective = "tardiness";
    if(Heuristic == "none")
      Heuristic = "osp-t";
  } else if(Type == "dyn") {
    Objective = "tardiness";
    if(Heuristic == "none")
      Heuristic = "osp-t";
  }

  if(int_param[0]  != NOVAL) UBinit      = int_param[0];
  if(int_param[1]  != NOVAL) LBinit      = int_param[1];
  if(int_param[2]  != NOVAL) Checked     = int_param[2];
  if(int_param[3]  != NOVAL) Seed        = int_param[3];
  if(int_param[4]  != NOVAL) Cutoff      = int_param[4];
  if(int_param[5]  != NOVAL) Dichotomy   = int_param[5];
  if(int_param[6]  != NOVAL) Base        = int_param[6];
  if(int_param[7]  != NOVAL) Randomized  = int_param[7]; 
  if(int_param[8]  != NOVAL) Verbose     = int_param[8];
  if(int_param[9]  != NOVAL) Optimise    = int_param[9]; 
  if(int_param[10] != NOVAL) Rngd        = int_param[10];
  if(int_param[13] != NOVAL) Hlimit      = int_param[13]; 
  if(int_param[14] != NOVAL) InitBound   = int_param[14]; 
  if(int_param[15] != NOVAL) Neighbor    = int_param[15]; 
  if(int_param[16] != NOVAL) InitStep    = int_param[16]; 
  if(int_param[17] != NOVAL) FixTasks    = int_param[17]; 
  if(int_param[18] != NOVAL) OrderTasks  = int_param[18]; 
  if(int_param[19] != NOVAL) NgdType     = int_param[19]; 
  if(int_param[20] != NOVAL) FD_learning     = int_param[20];
  if(int_param[21] != NOVAL) forgetall     = int_param[21];
  if(int_param[22] != NOVAL) reduce_clauses  = int_param[22];
  if(int_param[23] != NOVAL) orderedExploration  = int_param[23];
  if(int_param[24] != NOVAL) lazy_generation  = int_param[24];
  if(int_param[25] != NOVAL) semantic_learning  = int_param[25];
  if(int_param[26] != NOVAL) simple_learn  = int_param[26];
//  if(int_param[27] != NOVAL) { std::cout <<" No longer used! " << std::endl; exit(1); //max_nogood_size  = int_param[27];
  //}
  if(int_param[27] != NOVAL) max_nogood_size  = int_param[27];
  if(int_param[28] != NOVAL) bounded_by_decision  = int_param[28];
  if(int_param[29] != NOVAL) forget_relatedto_nogood_size  = int_param[29];
  if(int_param[30] != NOVAL) forget_retatedto_backjump  = int_param[30];
  if(int_param[31] != NOVAL) hard_keep  = int_param[31];
  if(int_param[32] != NOVAL) hard_forget  = int_param[32];
  if(int_param[33] != NOVAL) keep_when_size  = int_param[33];
  if(int_param[34] != NOVAL) keep_when_bjm  = int_param[34];
  if(int_param[35] != NOVAL) keeplearning_in_bb  = int_param[35];
  if(int_param[36] != NOVAL) simulaterestart  = int_param[36];
  if(int_param[37] != NOVAL) nogood_based_weight  = int_param[37];
  if(int_param[38] != NOVAL) weight_history  = int_param[38];
  if(int_param[39] != NOVAL) fixedForget  = int_param[39];
  if(int_param[40] != NOVAL) fixedlimitSize  = int_param[40];
  if(int_param[41] != NOVAL) fixedLearntSize  = int_param[41];
  if(int_param[42] != NOVAL) prob_forget  = int_param[42];
  if(int_param[43] != NOVAL) forgetdecsize  = int_param[43];
  if(int_param[44] != NOVAL) vsids  = int_param[44];
  if(int_param[45] != NOVAL) autoconfig  = int_param[45];
  if(int_param[46] != NOVAL) autoconfigublimit  = int_param[46];
  if(int_param[47] != NOVAL) autoconfiglblimit  = int_param[47];
  if(int_param[48] != NOVAL) limitresetpolicy  = int_param[48];
  if(int_param[49] != NOVAL) taskweight  = int_param[49];
  if(int_param[50] != NOVAL) adaptsize  = int_param[50];
  if(int_param[51] != NOVAL) adaptforget  = int_param[51];
  if(int_param[52] != NOVAL) repeatdicho  = int_param[52];
  if(int_param[53] != NOVAL) lbCutoff  = int_param[53];
  if(int_param[54] != NOVAL) sizeocc  = int_param[54];



  if (keep_when_bjm || keep_when_size)
	  forgetall=0;

  if(strcmp(str_param[0 ],"nil")) Heuristic  = str_param[0];
  if(strcmp(str_param[1 ],"nil")) Policy     = str_param[1];
  if(strcmp(str_param[2 ],"nil")) Factor     = atof(str_param[2]);
  if(strcmp(str_param[3 ],"nil")) Decay      = atof(str_param[3]);

  if(strcmp(str_param[5 ],"nil")) Value      = str_param[5];
  if(strcmp(str_param[6 ],"nil")) DValue     = str_param[6];
  if(strcmp(str_param[7 ],"nil")) IValue     = str_param[7];
  if(strcmp(str_param[8 ],"nil")) Objective  = str_param[8];
  if(strcmp(str_param[9 ],"nil")) Algorithm  = str_param[9 ];
  if(strcmp(str_param[10],"nil")) Presolve   = str_param[10];
  if(strcmp(str_param[11 ],"nil")) BandBPolicy     = str_param[11];
  if(strcmp(str_param[12],"nil")) Forgetfulness    = atof(str_param[12]);
  if(strcmp(str_param[13],"nil")) Forgetfulness_retated_to_backjump    = atof(str_param[13]);
  if(strcmp(str_param[14],"nil")) BBforgetfulness    = atof(str_param[14]);

}

void ParameterList::initialise(SchedulingSolver *s) {

  solver = s;
  if(int_param[11] != NOVAL) NodeBase    = int_param[11];

  // since the model is different for no-wait, we use a different factor
  unsigned long long int NodeFactor = 20000000;
  NodeCutoff = (NodeFactor * NodeBase);
  if(int_param[12] != NOVAL) NodeCutoff  = int_param[12]; 

  if(Policy == "luby")
    PolicyRestart = LUBY;
  else if(Policy == "geom")
    PolicyRestart = GEOMETRIC;
  else
    PolicyRestart = NORESTART;

  if(BandBPolicy == "luby")
	  BandBPolicyRestart = LUBY;
  else if(BandBPolicy == "geom")
	  BandBPolicyRestart = GEOMETRIC;
  else
	  BandBPolicyRestart = NORESTART;
}


std::ostream& ParameterList::print(std::ostream& os) {
  os << " c +==============[ parameters ]===============+" << std::endl;
  os << std::left << std::setw(30) << " c | data file " << ":" << std::right << std::setw(15) << data_file_name << " |" << std::endl;
  os << std::left << std::setw(30) << " c | type " << ":" << std::right << std::setw(15) << Type << " |" << std::endl;
  os << std::left << std::setw(30) << " c | learning " << ":" << std::right << std::setw(15) << FD_learning << " |" << std::endl;
  os << std::left << std::setw(30) << " c | orderedExploration " << ":" << std::right << std::setw(15) << orderedExploration << " |" << std::endl;
  os << std::left << std::setw(30) << " c | lazy_generation " << ":" << std::right << std::setw(15) << lazy_generation << " |" << std::endl;
  os << std::left << std::setw(30) << " c | semantic_learning " << ":" << std::right << std::setw(15) << semantic_learning << " |" << std::endl;
  os << std::left << std::setw(30) << " c | simple_learn " << ":" << std::right << std::setw(15) << simple_learn << " |" << std::endl;
  os << std::left << std::setw(30) << " c | max_nogood_size " << ":" << std::right << std::setw(15) << max_nogood_size << " |" << std::endl;
  os << std::left << std::setw(30) << " c | adaptsize " << ":" << std::right << std::setw(15) << adaptsize << " |" << std::endl;
  os << std::left << std::setw(30) << " c | adaptforget " << ":" << std::right << std::setw(15) << adaptforget << " |" << std::endl;
  os << std::left << std::setw(30) << " c | repeatdicho " << ":" << std::right << std::setw(15) << repeatdicho << " |" << std::endl;
  os << std::left << std::setw(30) << " c | bounded_by_decision " << ":" << std::right << std::setw(15) << bounded_by_decision << " |" << std::endl;
  os << std::left << std::setw(30) << " c | forget all clauses " << ":" << std::right << std::setw(15) << (forgetall? "yes" : "no") << " |" << std::endl;
  os << std::left << std::setw(30) << " c | hard_keep " << ":" << std::right << std::setw(15) << (hard_keep? "yes" : "no") << " |" << std::endl;
  os << std::left << std::setw(30) << " c | hard_forget " << ":" << std::right << std::setw(15) << (hard_forget? "yes" : "no") << " |" << std::endl;
  os << std::left << std::setw(30) << " c | keep_when_size " << ":" << std::right << std::setw(15) << keep_when_size << " |" << std::endl;
  os << std::left << std::setw(30) << " c | keep_when_bjm" << ":" << std::right << std::setw(15) << keep_when_bjm << " |" << std::endl;
  os << std::left << std::setw(30) << " c | keeplearning_in_bb" << ":" << std::right << std::setw(15) <<   keeplearning_in_bb << " |" << std::endl;
  os << std::left << std::setw(30) << " c | simulaterestart" << ":" << std::right << std::setw(15) <<   simulaterestart << " |" << std::endl;
  os << std::left << std::setw(30) << " c | weight_history" << ":" << std::right << std::setw(15) <<   weight_history << " |" << std::endl;
  os << std::left << std::setw(30) << " c | fixedForget" << ":" << std::right << std::setw(15) <<   fixedForget << " |" << std::endl;
  os << std::left << std::setw(30) << " c | fixedlimitSize" << ":" << std::right << std::setw(15) <<   fixedlimitSize << " |" << std::endl;
  os << std::left << std::setw(30) << " c | fixedLearntSize" << ":" << std::right << std::setw(15) <<   fixedLearntSize << " |" << std::endl;
  os << std::left << std::setw(30) << " c | prob_forget" << ":" << std::right << std::setw(15) <<   prob_forget << " |" << std::endl;
  os << std::left << std::setw(30) << " c | forgetdecsize" << ":" << std::right << std::setw(15) <<   forgetdecsize << " |" << std::endl;
  os << std::left << std::setw(30) << " c | vsids" << ":" << std::right << std::setw(15) <<   vsids << " |" << std::endl;
  os << std::left << std::setw(30) << " c | autoconfig" << ":" << std::right << std::setw(15) <<   autoconfig << " |" << std::endl;
  os << std::left << std::setw(30) << " c | autoconfigublimit" << ":" << std::right << std::setw(15) <<   autoconfigublimit << " |" << std::endl;
  os << std::left << std::setw(30) << " c | autoconfiglblimit" << ":" << std::right << std::setw(15) <<   autoconfiglblimit << " |" << std::endl;
  os << std::left << std::setw(30) << " c | limitresetpolicy" << ":" << std::right << std::setw(15) <<   limitresetpolicy << " |" << std::endl;
  os << std::left << std::setw(30) << " c | taskweight" << ":" << std::right << std::setw(15) <<   taskweight << " |" << std::endl;
  os << std::left << std::setw(30) << " c | nogood_based_weight" << ":" << std::right << std::setw(15) <<   nogood_based_weight << " |" << std::endl;
  os << std::left << std::setw(30) << " c | reduce learnt clause " << ":" << std::right << std::setw(15) << (reduce_clauses? "yes" : "no") << " |" << std::endl;
  os << std::left << std::setw(30) << " c | use size * occ as a reduction strategy? %" << ":" << std::right << std::setw(15) << (sizeocc? "yes" : "no")<< " |" << std::endl;
  os << std::left << std::setw(30) << " c | clause forgetfulness %" << ":" << std::right << std::setw(15) << Forgetfulness << " |" << std::endl;
  os << std::left << std::setw(30) << " c | backjump forgetfulness %" << ":" << std::right << std::setw(15) << Forgetfulness_retated_to_backjump << " |" << std::endl;
  os << std::left << std::setw(30) << " c | B&B forgetfulness %" << ":" << std::right << std::setw(15) << BBforgetfulness << " |" << std::endl;
  os << std::left << std::setw(30) << " c | static backjump forgetfulness%" << ":" << std::right << std::setw(15) << forget_retatedto_backjump << " |" << std::endl;
  os << std::left << std::setw(30) << " c | forget bounded by nogood size" << ":" << std::right << std::setw(15) << forget_relatedto_nogood_size  << " |" << std::endl;
  os << std::left << std::setw(30) << " c | seed " << ":" << std::right << std::setw(15) << Seed << " |" << std::endl;
  os << std::left << std::setw(30) << " c | greedy iterations " << ":" << std::right << std::setw(15) << InitBound << " |" << std::endl;
  os << std::left << std::setw(30) << " c | use initial probe " << ":" << std::right << std::setw(15) << (InitStep ? "yes" : "no") << " |" << std::endl;
  os << std::left << std::setw(30) << " c | time cutoff " << ":" << std::right << std::setw(15) << Cutoff << " |" << std::endl;
  os << std::left << std::setw(30) << " c | lb time cutoff " << ":" << std::right << std::setw(15) << lbCutoff << " |" << std::endl;
  os << std::left << std::setw(30) << " c | node cutoff " << ":" << std::right << std::setw(15) << NodeCutoff << " |" << std::endl;
  os << std::left << std::setw(30) << " c | dichotomy " << ":" << std::right << std::setw(15) << (Dichotomy ? "yes" : "no") << " |" << std::endl;
  os << std::left << std::setw(30) << " c | Dicho restart policy " << ":" << std::right << std::setw(15) << Policy << " |" << std::endl;
  os << std::left << std::setw(30) << " c | B&B restart policy " << ":" << std::right << std::setw(15) << BandBPolicy << " |" << std::endl;
  os << std::left << std::setw(30) << " c | base " << ":" << std::right << std::setw(15) << Base << " |" << std::endl;
  os << std::left << std::setw(30) << " c | factor " << ":" << std::right << std::setw(15) << Factor << " |" << std::endl;
  os << std::left << std::setw(30) << " c | heuristic " << ":" << std::right << std::setw(15) << Heuristic << " (" << abs(Randomized) << ")" << " |" << std::endl;
  os << std::left << std::setw(30) << " c | value ord. (init step) " << ":" << std::right << std::setw(15) << IValue << " |" << std::endl;
  os << std::left << std::setw(30) << " c | value ord. (dichotomy) " << ":" << std::right << std::setw(15) << DValue << " |" << std::endl;
  os << std::left << std::setw(30) << " c | value ord. (optim) " << ":" << std::right << std::setw(15) << Value << " |" << std::endl;
  os << std::left << std::setw(30) << " c | randomization " << ":" ;
     
  if(Randomized<-1) 
    os << std::right
       << std::setw(12) << "i-shuff random (" << std::setw(2) << -Randomized << ")" << " |" << std::endl;
  else if(Randomized>1) 
    os << std::right
       << std::setw(12) << "shuff & random (" << std::setw(2) << Randomized << ")" << " |" << std::endl;
  else if(Randomized==1) 
    os << std::right
       << std::setw(15) << "shuffled" << " |" << std::endl;
  else if(Randomized==0) 
    os << std::right
       << std::setw(15) << "not random" << " |" << std::endl;
  else 
    os << std::right
       << std::setw(15) << "init shuffle" << " |" << std::endl;
  
  os << " c +==============[ parameters ]===============+" << std::endl;
  
  return os;
}


Instance::Instance(ParameterList& params) {

  DBG("Build instance %s\n", params.data_file);

  dtp_nodes = 0;
  
  setup_time    = NULL;
  time_lag[0]   = NULL;
  time_lag[1]   = NULL;
  jsp_duedate   = NULL;
  jsp_latecost  = NULL;
  jsp_earlycost = NULL;
  jsp_floatcost = NULL;

  max_makespan  = INFTY;
  
  if(params.Type == "osp") {
    osp_readData( params.data_file );
  } else if(params.Type == "sds") {
    sds_readData( params.data_file );
  } else if(params.Type == "jtl") {
    jtl_readData( params.data_file );
  } else if(params.Type == "now" || params.Type == "now2") {
    now_readData( params.data_file );
  } else if(params.Type == "jla") {
    jla_readData( params.data_file );
  } else if(params.Type == "tsp") {
    tsp_readData( params.data_file );
  } else if(params.Type == "fsp") {
    fsp_readData( params.data_file );
  } else if(params.Type == "pfsp") {
    fsp_readData( params.data_file );
  } else if(params.Type == "jsp") {
    jsp_readData( params.data_file );
  } else if(params.Type == "jet") {
    jet_readData( params.data_file );
  } else if(params.Type == "dyn") {
    dyn_readData( params.data_file, params.Precision );
  } else if(params.Type == "dtp") {
    dtp_readData( params.data_file );
  } 
}

Instance::~Instance() {
  int i, j;

  if(hasSetupTime()) {
    for(i=0; i<nMachines(); ++i) {
      for(j=0; j<nJobs(); ++j) {
	delete [] setup_time[i][j];
      }
      delete [] setup_time[i];
    }
    delete [] setup_time;
  }

  if(hasTimeLag()) {
    for(i=0; i<nJobs(); ++i) {
      delete [] time_lag[0][i];
      delete [] time_lag[1][i];
    }
    delete [] time_lag[0];
    delete [] time_lag[1];
  }

  if(hasJobDueDate()) {
    delete [] jsp_duedate;
  }

  if(hasLateCost()) {
    delete [] jsp_latecost;
  }

  if(hasEarlyCost()) {
    delete [] jsp_earlycost;
  }
}


int straight_compar(const void *x, const void *y) {
  int a = *(int*)x;
  int b = *(int*)y;
  return(a==b ? 0 : (a<b ? -1 : 1));
}

void Instance::get_placement(const int x, const int y, Vector<int>& intervals) 
{  
  // we compute the allowed/forbidden start times of y relative to x
  // (like 'y in [x+30, x+70])

  // stores the start of each forbidden interval
  Vector<int> forbidden_starts;
  // stores the end of each forbidden interval
  Vector<int> forbidden_ends;

  // for each machine M the starting time of the task processed on M 
  // for job x
  int *timetable_x = new int[nMachines()];
  int *timetable_y = new int[nMachines()];

  // for each machine M the duration of the task processed on M
  // for job y
  int *duration_x = new int[nMachines()];
  int *duration_y = new int[nMachines()];


  int dur = 0;
  for(int k=0; k<nTasksInJob(x); ++k) {
    int t = getJobTask(x,k);
    for(int i=0; i<nMachines(t); ++i) {
      int m = getMachine(t,i);
      duration_x[m] = getDuration(t);
      timetable_x[m] = dur;
    }
    dur += getDuration(t);
  }
  dur = 0;
  for(int k=0; k<nTasksInJob(y); ++k) {
    int t = getJobTask(y,k);
    for(int i=0; i<nMachines(t); ++i) {
      int m = getMachine(t,i);
      duration_y[m] = getDuration(t);
      timetable_y[m] = dur;
    }
    dur += getDuration(t);
  }



  // compute all the forbidden intervals
  for(int k=0; k<nMachines(); ++k) {
    // forbidden interval for machine k
    // y cannot start in [st_x-duration_y+1, st_x+duration_x-1]
    // (since x is using machine k at this time)
    forbidden_starts.add(timetable_x[k]-timetable_y[k]-duration_y[k]+1);
    forbidden_ends.add(timetable_x[k]-timetable_y[k]+duration_x[k]);
  }


  // Now the cool part, we want to compute the 'real' intervals, since
  // some that we just computed can be merged if they overlap

  // we sort the intervals ends
  qsort(forbidden_starts.stack_, forbidden_starts.size, sizeof(int), straight_compar);
  qsort(forbidden_ends.stack_, forbidden_ends.size, sizeof(int), straight_compar);

  unsigned int i=0, j=0;
  int current = 0;    
  
  // now we can go forward from the earliest interval to latest
  while(i<forbidden_starts.size && j<forbidden_ends.size) {
    if(forbidden_starts[i]<forbidden_ends[j]) {
      ++i;

      // when we see the start of an interval, the current number
      // of machine that forbids this start time increases
      if(current++==0) {
	// if it goes from 0 to 1, it is the start of a forbidden interval
	intervals.add(forbidden_starts[i-1]-1);
      }
    } else if(forbidden_starts[i]>forbidden_ends[j]) {
      ++j;

      // when we see the end of an interval, the current number
      // of machine that forbids this start time decreases
      if(--current==0) {
	// if it goes from 1 to 0, it is the end of a forbidden interval
	intervals.add(forbidden_ends[j-1]);
      }
    } else {
      ++i;
      ++j;
    }
  }

  intervals.add(forbidden_ends.back());

//   int num__intervals = intervals.size/2+1;
//   int *max__intervals = new int[num__intervals];
//   int *min__intervals = new int[num__intervals];
//   min__intervals[0] = -NOVAL;
//   max__intervals[num__intervals-1] = NOVAL;
//   int k=0;
//   for(int i=0; i<num__intervals-1; ++i) {
//     max__intervals[i] = intervals[k++];
//     min__intervals[i+1] = intervals[k++];
//   }

}

void Instance::get_placement2(const int x, const int y) {
  // compute x's and y's lengths.
  int x_length=0, y_length=0;

  for(int k=0; k<nTasksInJob(x); ++k) {
    x_length += getDuration(getJobTask(x,k));
  }

  for(int k=0; k<nTasksInJob(y); ++k) {
    y_length += getDuration(getJobTask(y,k));
  }



  int *timetable_x_start = new int[nMachines()];
  int *timetable_x_end = new int[nMachines()];

  int *timetable_y_start = new int[nMachines()];
  int *timetable_y_end = new int[nMachines()];

  int *duration_x = new int[nMachines()];
  int *duration_y = new int[nMachines()];

  // the slack is the maximum right shift of y staying right of the given machine
  int *slack = new int[nMachines()];
  // the delay is the minimum right shift of y to get past the given machine
  int *delay = new int[nMachines()];

  //BitSet *timetable_y = new BitSet[nMachines()];

  
  int dur = 0;
  for(int k=0; k<nTasksInJob(x); ++k) {
    int t = getJobTask(x,k);
    for(int i=0; i<nMachines(t); ++i) {
      int m = getMachine(t,i);
      duration_x[m] = getDuration(t);
      timetable_x_start[m] = dur;
      timetable_x_end[m] = dur+getDuration(t);
    }
    dur += getDuration(t);
  }

  dur = -y_length;
  for(int k=0; k<nTasksInJob(y); ++k) {
    int t = getJobTask(y,k);
    for(int i=0; i<nMachines(t); ++i) {
      int m = getMachine(t,i);
      duration_y[m] = getDuration(t);
      timetable_y_start[m] = dur;
      timetable_y_end[m] = dur+getDuration(t);
    }
    dur += getDuration(t);
  }

  while(true) {
    int lb_shift=INFTY;
    int ub_shift=0;

    for(int i=0; i<nMachines(); ++i) {
      slack[i] = timetable_x_start[i]-timetable_y_end[i];
      delay[i] = timetable_x_end[i]-timetable_y_start[i];
      if(slack[i]<0) slack[i] = INFTY;

      if(slack[i]<lb_shift) lb_shift=slack[i];
      if(delay[i]>ub_shift) ub_shift=delay[i];
    }
  
    std::cout << "job" << x << ": " << x_length << " ";
    for(int k=0; k<nTasksInJob(x); ++k) {
      int t = getJobTask(x,k);
      for(int i=0; i<nMachines(t); ++i) {
	int m = getMachine(t,i);
	std::cout << "[" << std::setw(4) << timetable_x_start[m] << ":"
		  << std::setw(2) << getDuration(t) 
		  << " " << m
		  << "]";
      }
    }
    std::cout << std::endl;
    
    std::cout << "job" << y << ": " << y_length << " ";
    for(int k=0; k<nTasksInJob(y); ++k) {
      int t = getJobTask(y,k);
      for(int i=0; i<nMachines(t); ++i) {
	int m = getMachine(t,i);
	std::cout << "[" << std::setw(4) << timetable_y_start[m] << ":"
		  << std::setw(2) << getDuration(t) 
		  << " " << m
		  << "]";
      }
    }
    std::cout << std::endl;

    std::cout << "job" << y << ": " << y_length << " ";
    for(int k=0; k<nTasksInJob(y); ++k) {
      int t = getJobTask(y,k);
      for(int i=0; i<nMachines(t); ++i) {
	int m = getMachine(t,i);
	std::cout << "[" << std::setw(4) << slack[m] << "," << std::setw(4) << delay[m] << "]";
      }
    }
    std::cout << std::endl;

    //std::cout << min_forced_shift << " " << max_accepted_shift << " " << max_jump_shift << std::endl;
  

    
    // the shift should be, for each machine, either less than the slack, or more than the delay
    // therefore, we compute a forbidden interval, between the minimum slack, and the maximum delay
    if(lb_shift>=0) {
      std::cout << lb_shift << "]";
    }
    if(ub_shift>=0) {
      std::cout << "[" << ub_shift ;
    }

    std::cout << std::endl;

    
    for(int i=0; i<nMachines(); ++i) {
      timetable_y_start[i] += ub_shift;
      timetable_y_end[i] += ub_shift;
    }
  }

  exit(1);
}

int Instance::addTask(const int dur, const int job, const int machine) {
  int index = duration.size();
  if(job >= 0) addTaskToJob(index, job);
  if(machine >= 0) addTaskToMachine(index, machine);
  duration.push_back(dur);
  due_date.push_back(INFTY);
  release_date.push_back(0);
  
  return index;
}

void Instance::addTaskToJob(const unsigned int index, const unsigned int j) {
  if(tasks_in_job.size() <= j) tasks_in_job.resize(j+1);
  if(jobs_of_task.size() <= index) jobs_of_task.resize(index+1);
  if(task_rank_in_job.size() <= index) task_rank_in_job.resize(index+1);
  tasks_in_job[j].push_back(index);
  jobs_of_task[index].push_back(j);
  pair_ x(j, tasks_in_job[j].size()-1);
  task_rank_in_job[index].push_back(x);
}

void Instance::addTaskToMachine(const unsigned int index, const unsigned int j) {
  if(tasks_in_machine.size() <= j) tasks_in_machine.resize(j+1);
  if(machines_of_task.size() <= index) machines_of_task.resize(index+1);
  if(task_rank_in_machine.size() <= index) task_rank_in_machine.resize(index+1);
  tasks_in_machine[j].push_back(index);
  machines_of_task[index].push_back(j);
  pair_ x(j, tasks_in_machine[j].size()-1);
  task_rank_in_machine[index].push_back(x);
}

int Instance::getSetupTime(const int k, const int i, const int j) const {
  // get the rank of task i in machine k
  int ri = 0;
  for(unsigned int x=0; x<task_rank_in_machine[i].size(); ++x)
    if(task_rank_in_machine[i][x].element == k) {
      ri = task_rank_in_machine[i][x].rank;
      break;
    }

  int rj = 0;
  for(unsigned int x=0; x<task_rank_in_machine[j].size(); ++x)
    if(task_rank_in_machine[j][x].element == k) {
      rj = task_rank_in_machine[j][x].rank;
      break;
    }
  
  return setup_time[k][ri][rj];
}

std::ostream& Instance::print(std::ostream& os) {
  os << " c " << (nJobs()) << " jobs, " 
     << nMachines() << " machines ("
     << nTasks() << " tasks)" << std::endl;
  for(int i=0; i<nJobs(); ++i) {
    if(nTasksInJob(i) > 1) {
      os << " c ";
      for(int j=1; j<nTasksInJob(i); ++j)
	os << "  t" << tasks_in_job[i][j-1] << "+" << (duration[tasks_in_job[i][j-1]]) 
	   << " <= t" << tasks_in_job[i][j];
      os << std::endl;
    }
  }
  for(int i=0; i<nMachines(); ++i) {
    if(tasks_in_machine[i].size() > 0) {
      os << " c machine" << i << ": t" << tasks_in_machine[i][0];
      for(unsigned int j=1; j<tasks_in_machine[i].size(); ++j)
	os << ", t" << tasks_in_machine[i][j];
      os << std::endl;
    }
  }

//   for(int i=0; i<task_rank_in_machine.size(); ++i) {
//     std::cout << "task_" << i << " is"; 
//     for(int j=0; j<task_rank_in_machine[i].size(); ++j) {
//       pair_ p = task_rank_in_machine[i][j];
//       std::cout << " the " << p.rank << "/" << getMachine(i,j) << "th in machine_" << p.element;
//       if(task_rank_in_machine[i][j].element != machines_of_task[i][j]) {
// 	std::cout << "INCONSISTENCY" << std::endl;
// 	exit(1);
//       }
//     }
//     std::cout << std::endl;
//   }

  return os;
}

int Instance::nDisjuncts() const {
  int n_disjuncts = 0;
  for(int i=0; i<nMachines(); ++i) {
    n_disjuncts += (nTasksInMachine(i) * (nTasksInMachine(i)-1))/2;
  }
  return n_disjuncts;
}

int Instance::nPrecedences() const {
  int n_precedences = 0;
  for(int i=0; i<nJobs(); ++i) {
    n_precedences += (nTasksInJob(i)-1);
  }
  if(hasTimeLag()) {
    //n_precedences *= 2;
    for(int i=0; i<nJobs(); ++i) 
      for(int j=1; j<nTasksInJob(i); ++j) 
	if(getMaxLag(i,j-1) >= 0) 
	  ++n_precedences;
  }
  return n_precedences;
}

double Instance::getNormalizer() const {
  double normalizer=0.0, cost;
  int i, j, job_dur;
  for(i=0; i<nJobs(); ++i) {
    job_dur = 0;
    for(j=0; j<nTasksInJob(i); ++j) job_dur += getDuration(getJobTask(i,j));
    if(hasFloatCost())
      cost = getJobFloatCost(i);
    else if(hasEarlyCost() || hasLateCost()) {
      cost = 0.0;
      if(hasEarlyCost()) cost += getJobEarlyCost(i);
      if(hasLateCost()) cost += getJobLateCost(i);
    } else cost = 1.0;
    normalizer += ((double)job_dur * cost);
  }
  return normalizer;
}

std::ostream& Instance::printStats(std::ostream& os) {
  os << " c +===============[ instance ]================+" << std::endl
     << " d " << std::left << std::setw(25)  << " NUMTASKS "      << std::right << std::setw(19) << nTasks() << std::endl
     << " d " << std::left << std::setw(25)  << " NUMJOBS "       << std::right << std::setw(19) << nJobs() << std::endl
     << " d " << std::left << std::setw(25)  << " NUMMACHINES "   << std::right << std::setw(19) << nMachines() << std::endl
     << " d " << std::left << std::setw(25)  << " NUMDISJUNCTS "  << std::right << std::setw(19) << nDisjuncts() << std::endl
     << " d " << std::left << std::setw(25)  << " NUMPRECEDENCES "<< std::right << std::setw(19) << nPrecedences() << std::endl
//     << " d " << std::left << std::setw(25)  << "LBMAKESPAN "    << std::right << std::setw(19) << lb_C_max << std::endl
//     << " d " << std::left << std::setw(25)  << "UBMAKESPAN "    << std::right << std::setw(19) << ub_C_max << std::endl
     << " c +===============[ instance ]================+" << std::endl;
  return os;
}

int Instance::getMakespanLowerBound() {
  int mkp = 0, length;
  for(int i=0; i<nJobs(); ++i) {
    length = 0;
    for(int j=0; j<nTasksInJob(i); ++j)
      length += getDuration(getJobTask(i,j));
    if(mkp < length) mkp = length;
  }
  for(int i=0; i<nMachines(); ++i) {
    length = 0;
    for(int j=0; j<nTasksInMachine(i); ++j)
      length += getDuration(getMachineTask(i,j));
    if(mkp < length) mkp = length;
  }

  DBG("Get instance's makespan lb (%d)\n", mkp);

  return mkp;
}

int Instance::getMakespanUpperBound(const int iterations) {


  if(max_makespan < INFTY) return max_makespan;

  int best_makespan = INFTY;
  if(!hasTimeLag()) {
    int current_job[nJobs()];
    int current_job_bound[nJobs()];
    int current_machine_bound[nMachines()];
    int current_machine_task[nMachines()];
    int ranks[nTasks()];
    int random_jobs[nTasks()+1];
    int m[nMachines()];
    int k=0, i, j, t, n=0;

    std::fill(current_job, current_job+nJobs(), 0);
    while(k<nTasks()) {
      ranks[n++] = k;
      for(i=0; i<nJobs(); ++i)
	if(current_job[i]<nTasksInJob(i)) {
	  random_jobs[k++] = i;
	  ++current_job[i];
	  //std::cout << " " << i ;
	}
    }
    //std::cout << std::endl;
    ranks[n] = nTasks();

    int iter = iterations;
  
    while(iter--) {
      std::fill(current_job, current_job+nJobs(), 0);
      std::fill(current_job_bound, current_job_bound+nJobs(), 0);
      std::fill(current_machine_bound, current_machine_bound+nMachines(), 0);
      std::fill(current_machine_task, current_machine_task+nMachines(), -1);
  
      int makespan = 0;

      if(iter<iterations-1)
	for(i=0; i<n; ++i) {
	  for(t=ranks[i]; t<ranks[i+1]; ++t) {
	    j = randint(ranks[i+1]-t);
	    k = random_jobs[t];
	    random_jobs[t] = random_jobs[ranks[i]+j];
	    random_jobs[ranks[i]+j] = k;
	  }
	}
      //      for(i=0; i<nTasks(); ++i) {
      //	j = randint(nTasks()-i);
      // 	k = random_jobs[i];
      // 	random_jobs[i] = random_jobs[i+j];
      // 	random_jobs[i+j] = k;
      //       }
    
      for(i=0; i<nTasks(); ++i) {

// 	for(int jj=0; jj<nJobs(); ++jj) {
// 	  std::cout << " " << std::setw(4) << current_job_bound[jj];
// 	}
// 	std::cout << std::endl;

// 	for(int jj=0; jj<nMachines(); ++jj) {
// 	  std::cout << " " << std::setw(4) << current_machine_bound[jj];
// 	}
// 	std::cout << std::endl;


	// pick the next job
	j = random_jobs[i];


	// find out which task is that
	t = getJobTask(j, current_job[j]);

	DBG("pick task t%d\n", t);

	// find out which machine is that
	for(k=0; k<nMachines(t); ++k) {
	  m[k] = getMachine(t,k);
	  DBG("  -> uses machine%d\n", m[k]);
	}
      
// 	std::cout << "pick task " << t << "(job=" << j 
// 		  << ", mach=" << m[0] << ")" << std::endl;


	// find the current timepoint for this job
	int j_mkp = current_job_bound[j];

	// find the current timepoint for this machine
	int m_mkp = current_machine_bound[m[0]];
	DBG("m%d = %d\n", m[0], current_machine_bound[m[0]]);
	for(k=1; k<nMachines(t); ++k)
	  if(m_mkp < current_machine_bound[m[k]]) {
	    m_mkp = current_machine_bound[m[k]];
	    DBG("m%d = %d\n", m[k], current_machine_bound[m[k]]);
	  }

	// get the start time for this task
	int timepoint = (j_mkp < m_mkp ? m_mkp : j_mkp);

	//	std::cout << "earliest start time = " << timepoint << std::endl;

	// check its release date
	if(getReleaseDate(t) > timepoint) timepoint = getReleaseDate(t);

	DBG("timepoint = %d\n", timepoint);

	// add setup time, if any
	if(hasSetupTime()) {
	  int setup = 0;
	  int setup_mk;
	  for(k=0; k<nMachines(t); ++k) {
	    if(current_machine_task[m[k]] >= 0) {
	      setup_mk = getSetupTime(m[k], current_machine_task[m[k]], t);
	      if(setup < setup_mk) setup = setup_mk;
	      DBG("setup = %d\n", setup_mk);
	    }
	  }
	  timepoint += setup;
	}

	// get the finish time for this task
	timepoint += getDuration(t);

	// update machin and job bounds
	for(k=0; k<nMachines(t); ++k) {
	  current_machine_bound[m[k]] = timepoint;
	  current_machine_task[m[k]] = t;
	}
	//current_machine_bound[m] = timepoint;
	current_job_bound[j] = timepoint;

	// get the final makespan
	if(makespan < timepoint) makespan = timepoint;

	++current_job[j];
      }
      if(best_makespan > makespan) best_makespan = makespan;

      //exit(1);
      //std::cout << "\t" << makespan << " " << best_makespan << std::endl;
    }
  } else {

    best_makespan = 0;
    for(int t=0; t<nTasks(); ++t) best_makespan += getDuration(t);

  }

  DBG("Get instance's makespan ub (%d)\n", best_makespan);

  return best_makespan;
}

int Instance::getEarlinessTardinessLowerBound(const int c_max) {
  return 0;
}
int Instance::getEarlinessTardinessUpperBound(const int c_max) {
  int i, ti, sum_ub = 0;

  if(hasEarlyCost()) 
    for(i=0; i<nJobs(); ++i) {
      ti = getLastTaskofJob(i);
      sum_ub += ((getJobDueDate(i) - (getReleaseDate(ti) + getDuration(ti)))*getJobEarlyCost(i));
    }
    
  if(hasLateCost()) 
    for(i=0; i<nJobs(); ++i) {
      ti = getLastTaskofJob(i);
      sum_ub += ((c_max - getJobDueDate(i))*getJobLateCost(i));
    }

  return sum_ub;
}


void Instance::osp_readData( const char* filename ) {

  DBG("Read (osp)%s\n", "");

  int opt, lb, i=0, j, k, nJobs, nMachines, dur, bufsize=1000;
   char buf[bufsize];
   std::ifstream infile( filename, std::ios_base::in );
	
   do {
     infile.getline( buf, bufsize, '\n' );
   } while( buf[0] == '#' );
	
   while( buf[i] != ' ' ) ++i;
   buf[i] = '\0';
   lb = atoi( buf );
	
   while( buf[i] == '\0' || buf[i] == ' ' || buf[i] == '*' ) ++i;
   j = i;
   while( buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\0' ) ++i;
   buf[i] = '\0';
   opt = atoi( &(buf[j]) );
	
   do {
     infile.get( buf[0] );
     if( buf[0] != '#' ) break;
     infile.getline( buf, bufsize, '\n' );
   } while( true );
   infile.unget();
	
   infile >> nJobs;
   infile >> nMachines;

   infile.getline( buf, bufsize, '\n' );
	
   do {
     infile.get( buf[0] );
     if( buf[0] != '#' ) break;
     infile.getline( buf, bufsize, '\n' );
   } while( true );
   infile.unget();
	
   k = 0;
   for(i=0; i<nJobs; ++i) {
     for(j=0; j<nMachines; ++j) {
       infile >> dur;
       addTask(dur, k, -1);

       addTaskToMachine(k, i);
       addTaskToMachine(k, nJobs+j);       

       ++k;
     }
   }
}

void Instance::sds_readData( const char* filename ) {

  DBG("Read (sds)%s\n", "");

  int i, j, nJobs, nMachines, nFamilies, dur, mach;
  std::string tag;
  std::ifstream infile( filename, std::ios_base::in );
	
  infile >> nMachines;
  infile >> nJobs;
  infile >> nFamilies;

  int **family_matrix = new int*[nFamilies+1];
  for(i=0; i<=nFamilies; ++i)
    family_matrix[i] = new int[nFamilies];
	
  setup_time = new int**[nMachines];
  for(i=0; i<nMachines; ++i) {
    setup_time[i] = new int*[nJobs];
    for(j=0; j<nJobs; ++j) {
      setup_time[i][j] = new int[nJobs];
    }
  }
	
  int **family = new int*[nJobs];
  for(i=0; i<nJobs; ++i) 
    family[i] = new int[nMachines];

  for(i=0; i<nJobs; ++i) {
		
    infile >> j;
    assert(j==nMachines);
		
    for(j=0; j<nMachines; ++j) {

      infile >> dur;
      infile >> mach;
      --mach;

      addTask(dur, i, mach);
      //addTaskToMachine(k++, mach);
			
      infile >> family[i][j];
      --family[i][j];
    }
  }
	
  for(i=0; i<=nFamilies; ++i)
    for(j=0; j<nFamilies; ++j)
      infile >> family_matrix[i][j];
	
  for(int k=0; k<nMachines; ++k) {
    for(i=0; i<nJobs; ++i) {
      for(j=0; j<nJobs; ++j) {
	setup_time[k][i][j] = family_matrix[1+family[i][k]][family[j][k]] ;
	std::cout << " " << setup_time[k][i][j];
      }
      std::cout << std::endl;
    }
    std::cout << std::endl;
  }
  
  int k=0;
  for(i=0; i<nJobs; ++i) {
    for(j=0; j<nMachines; ++j)
      release_date[k++] = family_matrix[0][family[i][j]];
  }	

}
void Instance::jtl_readData( const char* filename ) {

  DBG("Read (jtl)%s\n", "");

  int i, j, dur, mach, nJobs, nMachines, opt;
  std::string tag;
  char c;
  std::ifstream infile( filename, std::ios_base::in );
  
  infile >> nJobs;
  infile >> nMachines;

  for(i=0; i<nJobs; ++i) {
    for(j=0; j<nMachines; ++j) {

      infile >> mach;
      infile >> dur;

      addTask(dur, i, mach);
    }
  }


  infile >> tag;
  infile >> opt;
  infile.ignore( 100, '\n' );
  
  infile.get(c);
  assert( c == 'T' );
  infile.get(c);
  assert( c == 'L' );
  infile.get(c);
  assert( c == '=' );

  
  time_lag[0] = new int*[nJobs];
  time_lag[1] = new int*[nJobs];

  for(i=0; i<nJobs; ++i) {
    time_lag[0][i] = new int[nMachines];
    time_lag[1][i] = new int[nMachines];
    for(j=0; j<nMachines; ++j) {
      infile >> time_lag[0][i][j];
      infile >> time_lag[1][i][j];
    }
  }

}
void Instance::now_readData( const char* filename ) {

  DBG("Read (now)%s\n", "");

  int i, j, dur, mach, nJobs, nMachines;

  std::string tag;

  std::ifstream infile( filename, std::ios_base::in );

  infile >> nJobs;

  infile >> nMachines;

  for(i=0; i<nJobs; ++i) {

    for(j=0; j<nMachines; ++j) {

      infile >> mach;

      infile >> dur;

      addTask(dur, i, mach);

    }

  }

  time_lag[0] = new int*[nJobs];

  time_lag[1] = new int*[nJobs];

  for(i=0; i<nJobs; ++i) {

    time_lag[0][i] = new int[nMachines];

    time_lag[1][i] = new int[nMachines];

    for(j=0; j<nMachines; ++j) {

      time_lag[0][i][j] = 0;

      time_lag[1][i][j] = 0;

    }

  }

//   print(std::cout);
//   std::cout << std::endl;
 
//   Vector<int> intervals;

//   for(int i=0; i<nJobs; ++i) {
//     for(int j=i+1; j<nJobs; ++j) {
//       intervals.clear();
//       get_placement(i,j,intervals);
//       intervals.print(std::cout);
//     }
//     std::cout << std::endl;
//   }
//   //exit(1);
}
void Instance::jla_readData( const char* filename ) {

  DBG("Read (jla)%s\n", "");

  int i, j, dur, mach, nJobs, nMachines;
  std::ifstream infile( filename, std::ios_base::in );
  
  infile >> nJobs;
  infile >> nMachines;

  for(i=0; i<nJobs; ++i) {
    for(j=0; j<nMachines; ++j) {

      infile >> mach;
      infile >> dur;

      addTask(dur, i, mach);
    }
  }

}
void Instance::tsp_readData( const char* filename ) {

  DBG("Read (tsp)%s\n", "");

}
void Instance::fsp_readData( const char* filename ) {

  DBG("Read (fsp)%s\n", "");

  int dur;
  std::vector<int> duration;
  double obj;
  std::ifstream infile( filename, std::ios_base::in );
	
  infile >> obj;
  max_makespan = (int)obj;

  while( true ) {
    infile >> dur;
    if(!infile.good()) break;
    duration.push_back(dur);

    //std::cout << dur << std::endl;
  }

  int nJobs = duration.size()/2;

  for(int i=0; i<nJobs; ++i) {
    addTask(duration[i], i, 0);
    addTask(duration[i+nJobs], i, 1);
  }

  //this->print(std::cout);

  //exit(1);
}

void Instance::jsp_readData( const char* filename ) {

  DBG("Read (jsp)%s\n", "");

  int i, j, k, dur, mach;
  long int dump;
  std::string tag;
  std::ifstream infile( filename, std::ios_base::in );
	
  int nJobs;
  int nMachines;
  infile >> nJobs;
  infile >> nMachines;
	
  infile >> dump;
  infile >> dump;
	
  infile >> dump;
  infile >> dump;
	
  infile >> tag;

  assert( tag == "Times" );

  for(i=0; i<nJobs; ++i) {
    for(j=0; j<nMachines; ++j) {
      infile >> dur;
      addTask(dur, i, -1);
    }
  }
	
  infile >> tag;
  assert( tag == "Machines" );
  
  k = 0;
  for(i=0; i<nJobs; ++i) 
    for(j=0; j<nMachines; ++j) {
      infile >> mach;
      addTaskToMachine(k++, --mach);
    }
}

void Instance::jet_readData( const char* filename ) {

  DBG("Read (jet)%s\n", "");

  int i, j, dur, mach, nJobs, nMachines;
  std::string tag;
  std::ifstream infile( filename, std::ios_base::in );
	
  infile >> nJobs;
  infile >> nMachines;
	
  jsp_duedate = new int[nJobs];
  jsp_earlycost = new int[nJobs];
  jsp_latecost = new int[nJobs];
	
  for(i=0; i<nJobs; ++i) {
    for(j=0; j<nMachines; ++j) {
			
      infile >> mach;
      infile >> dur;
      addTask(dur, i, mach);

    }
		
    infile >> jsp_duedate[i];
    infile >> jsp_earlycost[i];
    infile >> jsp_latecost[i];
  }

}

void Instance::dyn_readData( const char* filename, const int precision ) {

  DBG("Read (dyn)%s\n", "");

  int i, j, k, dur, mach, nJobs, nMachines;
  std::string tag;
  std::ifstream infile( filename, std::ios_base::in );
	
  infile >> nJobs;
  infile >> nMachines;
	
  jsp_duedate = new int[nJobs];
  jsp_earlycost = new int[nJobs];
  jsp_latecost = new int[nJobs];
  jsp_floatcost = new double[nJobs];

  for(i=0; i<nJobs; ++i) {
    k = release_date.size();

    for(j=0; j<nMachines; ++j) {
      
      infile >> mach;
      infile >> dur;
      
      if(mach != -1 && dur != -1) addTask(dur, i, mach);
    }		

    infile >> jsp_duedate[i];
    infile >> j;
    //infile >> jsp_latecost[i];
    infile >> jsp_floatcost[i];
    
    setReleaseDate(k, j);
    //jsp_earlycost[i] = jsp_latecost[i];
  }


  double min_cost = jsp_floatcost[0];
  double max_cost = jsp_floatcost[0];
  for(i=1; i<nJobs; ++i) {
    if(min_cost > jsp_floatcost[i]) min_cost = jsp_floatcost[i];
    if(min_cost < jsp_floatcost[i]) max_cost = jsp_floatcost[i];
  }
  for(i=0; i<nJobs; ++i) {
    int approx = (int)((jsp_floatcost[i]/max_cost)*precision);
    jsp_earlycost[i] = jsp_latecost[i] = approx;
    //std::cout << approx << std::endl;
  }

}

void Instance::dtp_readData( const char* filename ) {

  DBG("Read (dtp)%s\n", "");

  char comment = '#';
  std::ifstream infile( filename, std::ios_base::in );

  while(true) {
    comment = infile.get();
    if(comment == '#') {
      infile.ignore(1000, '\n');
    } else break;
  }
  infile.unget();
  
  std::string exp;
  int i, j, x, y, d;
  int step = 0;

  while(true) {
    infile >> exp;
    if(!infile.good())  break;

    if(step == 0) {
      std::vector< Term > clause;
      dtp_clauses.push_back(clause);
      dtp_weights.push_back(randint(100)+1);
    }

    //

    if(step%2 == 0) {
      i = 0;
      while(exp[++i] != '-');
      x = atoi(exp.substr(1,i-1).c_str());
      j = i+2;
      while(exp[++j] != '<');
      y = atoi(exp.substr(i+2,j-i-2).c_str());
      d = atoi(exp.substr(j+2,10).c_str());

      Term t;
      t.X = x;
      t.Y = y;
      t.k = d;

      if(t.X > dtp_nodes) dtp_nodes = t.X;
      if(t.Y > dtp_nodes) dtp_nodes = t.Y;

      dtp_clauses.back().push_back(t);

      //std::cout << "t" << x << " - t" << y  << " <= " << d << std::endl << std::endl;
    }

    step = (step+1) % 4;
  }

    ++dtp_nodes;

  // for(int i=0; i<dtp_clauses.size(); ++i) {
  //   for(int j=0; j<dtp_clauses[i].size(); ++j) {
  //     std::cout << "t" << dtp_clauses[i][j].X << " - t" << dtp_clauses[i][j].Y << " <= " << dtp_clauses[i][j].k << " OR ";
  //   }
  //   std::cout << std::endl;
  // }


}



SchedulingSolver::SchedulingSolver(Instance *pb, 
				   ParameterList *pr, 
				   StatisticList* st) {

  data = pb;
  params = pr;
  stats = st;
  stats->solver = this;
  
  params->initialise(this);

  setup(); //inst, params, max_makespan);  

  //  stats->lower_bound = get_lb();//lower_bound;
  //  stats->upper_bound = get_ub();//upper_bound;
  
  //nogoods = NULL;
  pool = new SolutionPool();


}

std::ostream& SchedulingSolver::printStats(std::ostream& os) {
  os << " c +=================[ model ]=================+" << std::endl
     << " d " << std::left << std::setw(25)  << "LBMAKESPAN "    << std::right << std::setw(19) << lb_C_max << std::endl
     << " d " << std::left << std::setw(25)  << "UBMAKESPAN "    << std::right << std::setw(19) << ub_C_max << std::endl
     << " c +=================[ model ]=================+" << std::endl;
  return os;
}

void SchedulingSolver::setup() {

  int i,j,k, lb, ub, ti, tj, rki, rkj, hi, hj, aux;

//  std::cout << "learn ? " << params->FD_learning <<std::endl;

  lb_C_max = (params->LBinit<0 ? data->getMakespanLowerBound() : params->LBinit);
  ub_C_max = (params->UBinit<0 ? data->getMakespanUpperBound(params->InitBound) : params->UBinit);

  if(params->Objective == "tardiness") {
    int max_due_date = data->getJobDueDate(0);
    for(int i=1; i<data->nJobs(); ++i) {
      if(max_due_date < data->getJobDueDate(i))
	max_due_date = data->getJobDueDate(i);
    }
    if(max_due_date < ub_C_max) max_due_date = ub_C_max;
    ub_C_max += max_due_date;
  }

  //for(i=0; i<data->nJobs(); ++i)
  //ub_C_max += data->getDuration(data->getLastTaskofJob(i));

  lb_L_sum = data->getEarlinessTardinessLowerBound(ub_C_max);
  ub_L_sum = data->getEarlinessTardinessUpperBound(ub_C_max);
  int __capacity = 3000;
  if (data->nTasks() > 2500){
	  __capacity =10;
  }
  else if (data->nTasks() > 1500){
	  __capacity =300;
  }
  else if (data->nTasks() > 500){
	  __capacity =1000;
  }
  //__capacity=0;
  //parameters.capacityinVarRangeWithLearning=0;
  // create one variable per task
  for(i=0; i<data->nTasks(); ++i) {
    lb = data->getReleaseDate(i);
    ub = std::min(ub_C_max, data->getDueDate(i)) - data->getDuration(i);

    if(lb > ub) {
      std::cout << "INCONSISTENT" << std::endl;
      exit(1);
    }
    if (params->FD_learning)
    {

    	//Variable t(lb, ub);
    	Variable t(lb, ub,RANGE_VAR_WITHLEARNING);
    	static_cast<VariableRangeWithLearning*>(t.range_domain)->init( __capacity );

    	tasks.add(t);
    	if (params->lazy_generation)
    		add(DomainFaithfulness(t));
    	else add(t);
    }
    else
    {
    	Variable t(lb, ub);
    	tasks.add(t);
    	add(t);
    }

  }

  if (params->FD_learning)
  {
	  Variable x_cmax(lb_C_max, ub_C_max, RANGE_VAR_WITHLEARNING);
	  static_cast<VariableRangeWithLearning*>(x_cmax.range_domain)->init( __capacity);

	  C_max = x_cmax;

	  if (params->lazy_generation)
		  add(DomainFaithfulness(C_max));
  	else add(C_max);


  }
  else
  {
	  Variable x_cmax(lb_C_max, ub_C_max);
	  C_max = x_cmax;
	  add(C_max);
  }

  // precedence constraints
  for(i=0; i<data->nJobs(); ++i) 
    for(j=1; j<data->nTasksInJob(i); ++j) {
      ti = data->getJobTask(i,j-1);
      tj = data->getJobTask(i,j);
      if (params->FD_learning)
    	  add( ExplainedPrecedence(tasks[ti],
    			  (data->getDuration(ti) +
    					  (data->hasTimeLag() ? data->getMinLag(i,j-1) : 0)),
    					  tasks[tj]) );
      else
    	  add( Precedence(tasks[ti],
    			  (data->getDuration(ti) +
    					  (data->hasTimeLag() ? data->getMinLag(i,j-1) : 0)),
    					  tasks[tj]) );

    }

  // time lags constraints
  if(data->hasTimeLag()) {
    for(i=0; i<data->nJobs(); ++i) 
      for(j=1; j<data->nTasksInJob(i); ++j) if(data->getMaxLag(i,j-1) >= 0) {
	  ti = data->getJobTask(i,j-1);
	  tj = data->getJobTask(i,j);
	  if (params->FD_learning)
		  add(ExplainedPrecedence(tasks[tj],
				  -(data->getDuration(ti)+data->getMaxLag(i,j-1)),
				  tasks[ti]) );
	  else
		  add( Precedence(tasks[tj],
				  -(data->getDuration(ti)+data->getMaxLag(i,j-1)),
				  tasks[ti]) );

	}
  }

  start_from = tasks.size +1;
//  std::cout << "SET START FROM " << start_from << std::endl;


  Vector< Vector< Vector< Variable > > > tmp_map;

  tmp_map.initialise(data->nMachines(),data->nMachines());

  // mutual exclusion constraints
  for(k=0; k<data->nMachines(); ++k) {

	  tmp_map[k].initialise(data->nTasksInMachine(k),data->nTasksInMachine(k));

	  for(i=0; i<data->nTasksInMachine(k); ++i){

		  tmp_map[k][i].initialise(data->nTasksInMachine(k),data->nTasksInMachine(k));

      for(j=i+1; j<data->nTasksInMachine(k); ++j) {
	ti = data->getMachineTask(k,i);
	tj = data->getMachineTask(k,j);
	
	if(params->OrderTasks==1) {
	  rki = data->getRankInJob(ti);
	  rkj = data->getRankInJob(tj);
	  hi = data->getHeadInJob(ti);
	  hj = data->getHeadInJob(tj);
	  
	  if(rkj < rki || (rkj == rki && hj < hi))
	    {
	      aux = ti;
	      ti = tj;
	      tj = aux;
	    }
	} else if(params->OrderTasks==-1) {
      
	  aux = ti;
	  ti = tj;
	  tj = aux;

	}

//  	std::cout << "t" << ti << " <> t" << tj 
//  		  << " (" << data->getDuration(ti) << "." 
// 		  << data->getDuration(tj) << ")" 
// 		  << std::endl; 

	first_job.add(data->getJob(ti,0));
	second_job.add(data->getJob(tj,0));

	//first_task_of_disjunct.push_back(ti);
	//second_task_of_disjunct.push_back(tj);
	if (params->FD_learning)
		disjuncts.add( ExplainedReifiedDisjunctive( tasks[ti],
				tasks[tj],

				data->getDuration(ti)
				+(data->hasSetupTime() ? data->getSetupTime(k,ti,tj) : 0),
				data->getDuration(tj)
				+(data->hasSetupTime() ? data->getSetupTime(k,tj,ti) : 0) ) );
	else
		disjuncts.add( ReifiedDisjunctive( tasks[ti],
		//disjuncts.add( ReifiedDisjunctiveGlobal( tasks[ti],
				tasks[tj],

				data->getDuration(ti)
				+(data->hasSetupTime() ? data->getSetupTime(k,ti,tj) : 0),
				data->getDuration(tj)
				+(data->hasSetupTime() ? data->getSetupTime(k,tj,ti) : 0) ) );


	Vector< Variable > tasks_of_d;
	tasks_of_d.add(disjuncts.back().get_var());
	tasks_of_d.add(tasks[ti].get_var());
	tasks_of_d.add(tasks[tj].get_var());
	disjunct_map.add(tasks_of_d);

	//std::cout << " add bool for i j= " << i << " " << j << std::endl ;
	//std::cout << " bool = " << disjuncts.back().get_var().id() << std::endl ;

	tmp_map[k][i][j]=disjuncts.back().get_var();

      }
	  }
  }

  for(unsigned int i=0; i<disjuncts.size; ++i)
   add(Free(disjuncts[i]));
  //add( disjuncts );




//   //exit(1);


  for(i=0; i<data->nJobs(); ++i) {
    ti = data->getLastTaskofJob(i);

    if (params->FD_learning)
    	add(ExplainedPrecedence(tasks[ti], data->getDuration(ti), C_max));
    else
    	add(Precedence(tasks[ti], data->getDuration(ti), C_max));

  }
  start_from = tasks.size +1;
  initial_variablesize= variables.size;
  parameters.simulaterestart= params->simulaterestart;


  if (params->FD_learning)
  {
	  set_fdlearning_on(
			  params->FD_learning, params->reduce_clauses,params->orderedExploration,
			  params->lazy_generation, params->semantic_learning, params->simple_learn,
			  params->max_nogood_size, params->bounded_by_decision, params->Forgetfulness,
			  params->forget_relatedto_nogood_size , params->forget_retatedto_backjump ,params->Forgetfulness_retated_to_backjump,
			  params->hard_keep, params->hard_forget,params->keep_when_size,
			  params->keep_when_bjm ,  params->keeplearning_in_bb, params->simulaterestart,
			  params->nogood_based_weight, params->fixedForget , params-> fixedlimitSize ,
			  params-> fixedLearntSize ,params-> prob_forget ,params-> forgetdecsize ,
			  (unsigned int) params-> limitresetpolicy , params-> taskweight , params->adaptsize,
			  params->adaptforget, params->sizeocc
	  );

	  if (!params->vsids)
		  parameters.updatefailurescope=1;
  }



  //Clauses
  //Vector< Vector< Literal > > clauses;
  //Vector< Literal > new_clause;
  VarArray subsequence;

  for(k=0; k<data->nMachines(); ++k)
	  for(i=0; i<data->nTasksInMachine(k); ++i)
		  for(j=i+1; j<data->nTasksInMachine(k); ++j)
			  for(int l=j+1; l<data->nTasksInMachine(k); ++l) {


				  /*    			std::cout << " task i " << i << std::endl ;
    			std::cout << " task j " << j << std::endl ;
    			std::cout << " task l " << l << std::endl ;

    			std::cout << " bool i j" << tmp_map[k][i][j].id() << std::endl ;
    			std::cout << " bool j l" << tmp_map[k][j][l].id() << std::endl ;
    			std::cout << " bool i l" << tmp_map[k][i][l].id() << std::endl ;
    			std::cout << " \n "<< std::endl ;
				   */

				  /*    			new_clause.clear();
    			//lit <-- Not e_kij
    			lit =  2* tmp_map[k][i][j].id();
    			new_clause.add(lit);
    			//lit <-- Not e_kjl
    			lit =  2* tmp_map[k][j][l].id();
    			new_clause.add(lit);
    			//lit <-- e_kil
    			lit =  2* tmp_map[k][i][l].id()+1;
    			new_clause.add(lit);


    			new_clause.clear();
    			//lit <-- e_kij
    			lit =  2* tmp_map[k][i][j].id()+1;
    			new_clause.add(lit);
    			//lit <-- e_kjl
    			lit =  2* tmp_map[k][j][l].id()+1;
    			new_clause.add(lit);
    			//lit <-- Not e_kil
    			lit =  2* tmp_map[k][i][l].id();
    			new_clause.add(lit);
    			clauses.add(new_clause);


				   */

				  /*    			new_clause.clear();
    			new_clause.add(encode_boolean_variable_as_literal(tmp_map[k][i][j].id(),0));
    			new_clause.add(encode_boolean_variable_as_literal(tmp_map[k][j][l].id(),0));
    			new_clause.add(encode_boolean_variable_as_literal(tmp_map[k][i][l].id(),1));
    			clauses.add(new_clause);
    			new_clause.clear();
    			new_clause.add(encode_boolean_variable_as_literal(tmp_map[k][i][j].id(),1));
    			new_clause.add(encode_boolean_variable_as_literal(tmp_map[k][j][l].id(),1));
    			new_clause.add(encode_boolean_variable_as_literal(tmp_map[k][i][l].id(),0));
    			clauses.add(new_clause);

				   */


				  subsequence.clear();
				  subsequence.add(!tmp_map[k][i][j]);
				  subsequence.add(!tmp_map[k][j][l]);
				  subsequence.add(tmp_map[k][i][l]);
				//  add( BoolSum( subsequence, 1, INFTY) );

				  subsequence.clear();
				  subsequence.add(tmp_map[k][i][j]);
				  subsequence.add(tmp_map[k][j][l]);
				  subsequence.add(!tmp_map[k][i][l]);
				  //add( BoolSum( subsequence, 1, INFTY) );

			  }

  //add clauses
/*		for(int i=0; i<clauses.size; ++i) {
			base->add(clauses[i]);
		}
*/


  parameters.limitresetpolicy =  params->limitresetpolicy;


#ifdef _MONITOR
  monitor_list << tasks[15] << " "

#endif


//   if(data->hasJobDueDate()) {
//     // early/late bools - whether the last tasks are early or late
//     for(i=0; i<data->nJobs(); ++i) {
//       ti = data->getLastTaskofJob(i);
//       if(data->hasEarlyCost())
// 	earlybool.add(tasks[ti] < (data->getJobDueDate(i) - data->getDuration(ti)));
//       if(data->hasLateCost())
// 	latebool.add(tasks[ti] > (data->getJobDueDate(i) - data->getDuration(ti)));
//     }
    
//     //DG Added for calculating cost
//     VarArray sum_scope;
//     int weights[2*data->nJobs()+1];
//     int sum_ub = 0;
//     int n_weights = 0;

//     k=0;
//     if(data->hasEarlyCost()) {
//       for(i=0; i<data->nJobs(); ++i) {
// 	ti = data->getLastTaskofJob(i);
// 	Variable x(0, data->getJobDueDate(i)-data->getDuration(ti)); // amount of "earliness"
// 	add(x == (earlybool[i]*((tasks[ti]*-1) - (data->getDuration(ti) - data->getJobDueDate(i)))));
// 	sum_scope.add(x);
// 	weights[k++] = data->getJobEarlyCost(i);
// 	sum_ub += ((data->getJobDueDate(i) - (data->getReleaseDate(ti) + data->getDuration(ti)))*data->getJobEarlyCost(i));
//       }
//       n_weights += data->nJobs();
//     }
    
//     if(data->hasLateCost()) {
//       for(i=0; i<data->nJobs(); ++i) {
// 	ti = data->getLastTaskofJob(i);
// 	Variable x(0, ub_C_max-data->getJobDueDate(i));
// 	add(x == (latebool[i]*(tasks[ti] + (data->getDuration(ti) - data->getJobDueDate(i)))));
// 	sum_scope.add(x);
// 	weights[k++] = data->getJobLateCost(i);
// 	sum_ub += ((ub_C_max - data->getJobDueDate(i))*data->getJobLateCost(i));
//       }
//       n_weights += data->nJobs();
//     }
    
//     weights[n_weights] = 0;
    
//     Variable x_lsum(0, sum_ub);
//     L_sum = x_lsum;
//     add( L_sum == Sum(sum_scope, weights) );
//   }


//   // initialise last tasks
//   for(i=0; i<data->nJobs(); ++i) last_tasks.add(tasks[data->getLastTaskofJob(i)]);

//   // initialise searched vars
//   if(data->hasJobDueDate()) {
//     for(int i=0; i<earlybool.size(); ++i) SearchVars.add(earlybool[i]);
//     for(int i=0; i< latebool.size(); ++i) SearchVars.add(latebool [i]);
//     for(int i=0; i<data->nJobs(); ++i) SearchVars.add(tasks[data->getLastTaskofJob(i)]);
//   }
//   for(int i=0; i<disjuncts.size(); ++i) SearchVars.add(disjuncts[i]);
//   if(params->FixTasks)
//     for(int i=0; i<tasks.size(); ++i) SearchVars.add(tasks[i]);

//   lb_Depth = 0;
//   ub_Depth = disjuncts.size();

//   if(params->Objective == "depth") {
//     Variable depth(lb_Depth, ub_Depth);
//     Depth = depth;
//     VarArray scope;
//     for(int i=0; i<disjuncts.size()/2; ++i) {
//       add(disjuncts[i] == disjuncts[i+disjuncts.size()/2]);
//       scope.add(disjuncts[i]);
//     }
//     add(depth == Sum(scope));
//   }

		 //std::cout << " this  " << this << std::endl;

		  //std::cout << " b1491 :  " << variables[1491].get_domain() << std::endl;
  	  	  //exit(1);


		 // std::cout << " NOGOOD " << std::endl;
  //Cut 1:
  /*
  add(variables[346] == 0);
  add(variables[58] >=  807);
  add(variables[28]  <=  948);
  add(variables[161]   <=  954);
  add(variables[161]  >=  852);
*/
/*
  add(variables[346] == 0);
  add(variables[58] >=  807);
  //add(variables[28]  <=  948);
  add(variables[161]   <=  855);
  //add(variables[161]  >=  852);
*/


			// std::cout << " end setup with :  " << this << std::endl;
#ifdef _ASSIGNMENT_ORDER
		  if (params->FD_learning)
			  for (int i = 0; i< start_from; ++i)
				  (static_cast<VariableRangeWithLearning *> (variables[i].range_domain)) ->order = &assignment_rank;
#endif


  if (params->FD_learning)
	  if (params->autoconfig){
		  //		  std::cout << " c autoconfig"  << std::endl;
		  if (params->autoconfig==1)
			  parameters. fixedlimitSize=  data->nDisjuncts() * 6.5 ;
		  else
			  parameters. fixedlimitSize=  data->nDisjuncts() * params->autoconfig ;

		  if (parameters. fixedlimitSize > params->autoconfigublimit)
			  parameters. fixedlimitSize=params->autoconfigublimit;
		  else
			  if (parameters. fixedlimitSize < params->autoconfiglblimit)
				  parameters. fixedlimitSize = params->autoconfiglblimit ;

		  parameters.fixedLearntSize = (parameters.fixedlimitSize /3);
		  params->fixedLearntSize =   parameters.fixedLearntSize ;
		  params->fixedlimitSize =   parameters.fixedlimitSize ;

		  //		  std::cout << " c parameters.fixedlimitSize =" << parameters.fixedlimitSize << std::endl;
		  //		  std::cout << " c parameters.fixedLearntSize" << parameters.fixedLearntSize << std::endl;

	  }

}

void SchedulingSolver::initialise_heuristic (int update_weights){

	//std::cout << " c SchedulingSolver::initialise_heuristic" << update_weights << std::endl;
	delete heuristic;
	if (params->vsids){
	//(value_ordering == "minval+guided")
	heuristic= new GenericHeuristic< VSIDS<2>, Guided< MinValue > >(this);
	}
	else{
		if (params->taskweight)
			heuristic= new SchedulingWeightedDegree < TaskDomOverTaskWeight, Guided< MinValue >, 2 > (this, disjunct_map);
		else
			heuristic= new SchedulingWeightedDegree < TaskDomOverBoolWeight, Guided< MinValue >, 2 > (this, disjunct_map);
	}

	heuristic->initialise(sequence);

	if (update_weights){
		//std::cout << "error updating weights is not yet supported with  TaskDomOverTaskWeight" << std::endl;
		//exit(1);
		FailureCountManager* failmngr ;
		if (params->taskweight)
			failmngr = (FailureCountManager*)
			(((SchedulingWeightedDegree< TaskDomOverTaskWeight, Guided< MinValue >, 2 >*) heuristic)->var.manager);
		else
		failmngr = (FailureCountManager*)
	            		  (((SchedulingWeightedDegree< TaskDomOverBoolWeight, Guided< MinValue >, 2 >*) heuristic)->var.manager);

		//  failmngr->display(std::cout , true);

		for (int i=(_constraint_weight->size -1); i>= 0 ; --i){
			failmngr->constraint_weight[i]= (*_constraint_weight)[i];
		}

		for (int i=(_variable_weight->size -1 ); i>= 0 ; --i){
			failmngr->variable_weight[i]= (*_variable_weight)[i];
		}
	}
}

SchedulingSolver::~SchedulingSolver() {
#ifdef _CHECK_NOGOOD
#else
	if (_constraint_weight)
		delete _constraint_weight;
	if (_variable_weight)
		delete _variable_weight;
	if (pool)
		delete pool;
#endif

}


// void No_wait_Model::setup(Instance& inst, ParameterList *params, const int max_makespan) {

//   //if(type==0) {
//   int i,j,k, lb, ub, ti=0, tj=0;
//   std::vector<int> offset;
//   for(i=0; i<data->nJobs(); ++i) {
//     offset.push_back(0);
//     for(j=0; j<data->nTasksInJob(i)-1; ++j)
//       offset.push_back(offset.back() + data->getDuration(data->getJobTask(i,j)));
//   }

//   lb_C_max = data->getMakespanLowerBound();
//   if(max_makespan < 0) {
//     ub_C_max = 0;
//     for(i=0; i<data->nJobs(); ++i) {
//       ub_C_max += (offset[data->getLastTaskofJob(i)] + data->getDuration(data->getLastTaskofJob(i)));
//     }
//   } else ub_C_max = max_makespan;

//   data = &inst;

//   // create one variable per task
//   for(i=0; i<data->nJobs(); ++i) {

//     lb = data->getReleaseDate(data->getJobTask(i,0));
//     ub = std::min(ub_C_max, data->getDueDate(data->getLastTaskofJob(i))) 
//       - (offset[data->getLastTaskofJob(i)] + data->getDuration(data->getLastTaskofJob(i)));


//     if(lb > ub) {
//       std::cout << "INCONSISTENT" << std::endl;
//       exit(1);
//     }

//     Variable t(lb, ub);
//     tasks.add(t);
//   }

//   if(type==0) {

//     // mutual exclusion constraints
//     for(k=0; k<data->nMachines(); ++k) 
//       for(i=0; i<data->nTasksInMachine(k); ++i)
// 	for(j=i+1; j<data->nTasksInMachine(k); ++j) {
// 	  ti = data->getMachineTask(k,i);
// 	  tj = data->getMachineTask(k,j);
	  
// 	  first_task_of_disjunct.push_back(ti);
// 	  second_task_of_disjunct.push_back(tj);

// 	  disjuncts.add( Disjunctive( tasks[data->getJob(ti,0)],
				      
// 				      data->getDuration(ti)
// 				      +(data->hasSetupTime() ? data->getSetupTime(k,ti,tj) : 0)
// 				      +offset[ti]
// 				      -offset[tj],
				      
// 				      tasks[data->getJob(tj,0)],
				      
// 				      data->getDuration(tj)
// 				      +(data->hasSetupTime() ? data->getSetupTime(k,tj,ti) : 0)
// 				      +offset[tj]
// 				      -offset[ti] ) );
// 	}
//     add( disjuncts );
//   } else {
    
//     //       print(std::cout);
//     //   std::cout << std::endl;

//     if(true) {
//     Vector<int> intervals;
    
//     for(int i=0; i<data->nJobs(); ++i) {
//       for(int j=i+1; j<data->nJobs(); ++j) {
// 	intervals.clear();
// 	data->get_placement(i,j,intervals);
	
// 	int job_start = disjuncts.size();
// 	for(int k=0; k<intervals.size; k+=2) {

// 	  first_task_of_disjunct.push_back(i);
// 	  second_task_of_disjunct.push_back(j);

// 	  //disjuncts.add( Disjunctive(tasks[j], -intervals[k], tasks[i], intervals[k+1]) );
// 	  disjuncts.add( Disjunctive(tasks[i], intervals[k+1], tasks[j], -intervals[k]) );
// 	}
// 	int ds = disjuncts.size();
//   	for(int k=job_start; k<ds; ++k) {
// //  	  add(disjuncts[k-1] <= disjuncts[k]);

// 	  job_size.add(ds-job_start);
// 	  job_index.add(k-job_start);
//  	}

//       	//gen_disjuncts.add( GenDisjunctive(tasks[i], tasks[j], intervals) );

//       }
//       //std::cout << std::endl;
//     }
    
// //     job_size.print(std::cout);
// //     std::cout << std::endl;

// //     job_index.print(std::cout);
// //     std::cout << std::endl;

//     add(disjuncts);


//     } else {


//     Vector<int> intervals;
    
//     for(int i=0; i<data->nJobs(); ++i) {
//       for(int j=i+1; j<data->nJobs(); ++j) {
// 	intervals.clear();
// 	data->get_placement(i,j,intervals);
// 	//intervals.print(std::cout);
//       	gen_disjuncts.add( GenDisjunctive(tasks[i], tasks[j], intervals) );

//       }
//       //std::cout << std::endl;
//     }
    
//     add(gen_disjuncts);
//     //exit(1);
//     }
//   }



//   Variable x_cmax(lb_C_max, ub_C_max);
//   C_max = x_cmax;
//   for(i=0; i<data->nJobs(); ++i) {
//     ti = data->getLastTaskofJob(i);
//     k = data->getDuration(ti) + offset[ti];
//     add(Precedence(tasks[i], k, C_max));
//   }

  
//   if(disjuncts.size()>0) {
//     for(int i=0; i<disjuncts.size(); ++i) SearchVars.add(disjuncts[i]);
//   } 
//   if(gen_disjuncts.size()>0) {
//     for(int i=0; i<gen_disjuncts.size(); ++i) SearchVars.add(gen_disjuncts[i]);
//   }

  

// }


// void DTP_Model::setup(Instance& inst, ParameterList *params, const int max_makespan) {

//   ub_C_max = max_makespan; 
//   ub_Weight = 0;
//   for(int i=0; i<data->dtp_clauses.size(); ++i) {
//     ub_Weight += data->dtp_weights[i];
//   }
//   data->dtp_weights.push_back(0);

//   for(int i=0; i<data->dtp_nodes; ++i) {
//     Variable t(0, max_makespan);
//     tasks.add(t);
//   }

//   // x-y <= k
//   for(int i=0; i<data->dtp_clauses.size(); ++i) {
//     for(int j=0; j<2; ++j) {
//       SearchVars.add( Precedence( tasks[data->dtp_clauses[i][j].X], 
// 				  -data->dtp_clauses[i][j].k, 
// 				  tasks[data->dtp_clauses[i][j].Y] ) );
//     }
//     int k = SearchVars.size();
//     disjuncts.add( SearchVars[k-2] || SearchVars[k-1] );
//   }

//   // for(int i=0; i<disjuncts.size(); ++i) {
//   //   SearchVars.add(disjuncts[i]);
//   // }
  
//   Weight = Variable(0,ub_Weight);
//   add(Weight == (-Sum(disjuncts, &(data->dtp_weights[0])) + ub_Weight));
//   //  Weight = Sum(disjuncts, &(data->dtp_weights[0]));
//   //add(Maximise(Weight));
// }


int L_sum_Model::get_lb() {
  return lb_L_sum;
}

int L_sum_Model::get_ub() {
  return ub_L_sum;
}

int Depth_Model::get_lb() {
  return lb_Depth;
}

int Depth_Model::get_ub() {
  return ub_Depth;
}

int DTP_Model::get_lb() {
  return 0;
}

int DTP_Model::get_ub() {
  return ub_Weight;
}

int C_max_Model::get_lb() {
  return lb_C_max;
}

int C_max_Model::get_ub() {
  return ub_C_max;
}

Variable L_sum_Model::get_objective_var() {
  return L_sum;
}

Variable Depth_Model::get_objective_var() {
  return Depth;
}

Variable DTP_Model::get_objective_var() {
  return Weight;
}

Variable C_max_Model::get_objective_var() {
  return C_max;
}

int L_sum_Model::set_objective(const int obj) {
  //VariableInt *x_lsum = L_sum.getVariable();
  return (L_sum.set_max(obj) != FAIL_EVENT ? UNKNOWN : UNSAT);
}

int Depth_Model::set_objective(const int obj) {
//   //VariableInt *x_lsum = Depth.getVariable();
//   Depth.print(std::cout);
//   std::cout << std::endl;
//   Depth.getVariable()->print(std::cout);
//   std::cout << std::endl;
  return (Depth.set_max(obj) != FAIL_EVENT ? UNKNOWN : UNSAT);
}

int DTP_Model::set_objective(const int obj) {
  return (Weight.set_max(obj) != FAIL_EVENT ? UNKNOWN : UNSAT);
}

int C_max_Model::set_objective(const int obj) {
	if (params->FD_learning)
		return ( (static_cast<VariableRangeWithLearning *> (C_max.range_domain) ) -> set_max(obj,NULL) != FAIL_EVENT ? UNKNOWN : UNSAT);
	else
		return (C_max.set_max(obj) != FAIL_EVENT ? UNKNOWN : UNSAT);
}

int L_sum_Model::get_objective() {
  return L_sum.get_solution_min();
}

int Depth_Model::get_objective() {
  return Depth.get_solution_min();
}

int DTP_Model::get_objective() {
  return Weight.get_solution_min();
}

// double L_sum_Model::get_normalized_objective() {
//   double total_cost = 0, cost;
  // for(int i=0; i<latebool.size(); ++i) {
  //   cost = 0.0;
  //   if(earlybool[i].value()) {
  //     cost = (data->getJobDueDate(i) - (last_tasks[i].value()+data->getDuration(data->getLastTaskofJob(i))))*(data->hasFloatCost() ? data->getJobFloatCost(i) : data->getJobEarlyCost(i));
  //   } else if(latebool[i].value()) {
  //     cost = ((last_tasks[i].value()+data->getDuration(data->getLastTaskofJob(i))) - data->getJobDueDate(i))*(data->hasFloatCost() ? data->getJobFloatCost(i) : data->getJobLateCost(i));
  //   }
  //   total_cost += cost;
  // }
  
  // total_cost /= data->getNormalizer();

//   return total_cost;
// }

int C_max_Model::get_objective() {
  return C_max.get_solution_min();
}


SchedulingSolution::SchedulingSolution(SchedulingSolver *s) {
  //model = m;
  solver = s;

  int i, n=solver->variables.size;

  earlybool_value = NULL;
  latebool_value = NULL;
  task_min = NULL;
  task_max = NULL;
  ltask_value = NULL;
  disjunct_value = NULL;
  search_value = NULL;
  all_value = new int[n];

  for(i=0; i<n; ++i) {
    all_value[i] = solver->variables[i].get_solution_int_value();
//     std::cout << "store " ;
//     solver->variables[i]->print(std::cout);
//     std::cout << std::endl;
  }


  n = s->disjuncts.size;
  if(n) {
    disjunct_value = new int[n];
    for(i=0; i<n; ++i) disjunct_value[i] = s->disjuncts[i].get_solution_int_value();
  } 

  n = s->SearchVars.size;
  if(n) {
    search_value = new int[n];
    for(i=0; i<n; ++i) search_value[i] = s->SearchVars[i].get_solution_int_value();
  }

  if(s->data->hasJobDueDate()) {
    n = s->earlybool.size;
    if(n) {
      earlybool_value = new int[n];
      for(i=0; i<n; ++i) earlybool_value[i] = s->earlybool[i].get_solution_int_value();
    } 
    n = s->latebool.size;
    if(n) {
      latebool_value = new int[n];
      for(i=0; i<n; ++i) latebool_value[i] = s->latebool[i].get_solution_int_value();
    } 
    n = s->tasks.size;
    if(n) {
      task_min = new int[n];
      task_max = new int[n];
      for(i=0; i<n; ++i) {
	task_min[i] = s->tasks[i].get_solution_min();
	task_max[i] = s->tasks[i].get_solution_max();
      } 
    }

    n = s->last_tasks.size;
    if(n) {
      ltask_value = new int[n];
      for(i=0; i<n; ++i) {
	ltask_value[i] = s->last_tasks[i].get_solution_int_value();
      }
    }
  }
}


SchedulingSolution::~SchedulingSolution() {
  delete [] earlybool_value;
  delete [] latebool_value;
  delete [] task_min;
  delete [] task_max;
  delete [] ltask_value;
  delete [] disjunct_value;
  delete [] search_value;
  delete [] all_value;
}

void SchedulingSolution::guide_search() {
  
// //   std::cout << "guide search with ";
// //   print(std::cout);
// //   std::cout << std::endl;

// //   //solver->setGuidedOrdering(model->disjuncts, disjunct_value, "spl");
// //   if( ((SchedulingSolver*)solver)->params->Type == "now2" )
// //     solver->setGuidedBoundsOrdering(model->SearchVars, search_value);
// //   else
    
//     solver->setGuidedOrdering(model->SearchVars, search_value);

//   if(model->data->hasJobDueDate()) {
//     solver->setGuidedOrdering(model->earlybool, earlybool_value);
//     solver->setGuidedOrdering(model->latebool, latebool_value);
//     solver->setGuidedOrdering(model->last_tasks, ltask_value, "nbd");
//   }
}

void SchedulingSolution::guide_search_bounds() {
  
// //   std::cout << "guide bound search with ";
// //   print(std::cout);
// //   std::cout << std::endl;

// //   //solver->setGuidedOrdering(model->disjuncts, disjunct_value, "spl");
// //   if( ((SchedulingSolver*)solver)->params->Type == "now2" )
// //     solver->setGuidedBoundsOrdering(model->SearchVars, search_value);
// //   else
    
//     solver->setGuidedOrdering(model->SearchVars, search_value);
//   if(model->data->hasJobDueDate()) {
//     solver->setGuidedOrdering(model->earlybool, earlybool_value);
//     solver->setGuidedOrdering(model->latebool, latebool_value);
//     solver->setGuidedOrdering(model->last_tasks, ltask_value, "nbd");
//   }
}

double SchedulingSolution::distance(SchedulingSolution* s) {
  
  int i, //n = model->disjuncts.size;
    n = solver->SearchVars.size;
  double dist = 0.0;

  for(i=0; i<n; ++i) {
    //dist += (double)(disjunct_value[i] != s->disjunct_value[i]);
    dist += (double)(search_value[i] != s->search_value[i]);
  } 
  if(solver->data->hasJobDueDate()) {
    n = solver->last_tasks.size;
    for(i=0; i<n; ++i) {dist += 
	((ltask_value[i] - s->ltask_value[i])*(ltask_value[i] - s->ltask_value[i]));
    }
  }
  
  return sqrt(dist);
}

std::ostream& SchedulingSolution::print(std::ostream& os, std::string type) {
  int i, //n = solver->disjuncts.size;
    n = solver->SearchVars.size;

//   for(i=0; i<n; ++i) {
//     os << disjunct_value[i] ;
//   } 
//   os << std::endl;

  if(type == "fsp") {
    int j, m = solver->data->nJobs(), k = 0;
    int *rank = new int[m];
    int *order = new int[m];
    std::fill(rank, rank+m, 0);

    for(i=0; i<m-1; ++i) {
      for(j=i+1; j<m; ++j) {
	//if(!disjunct_value[k++]) ++rank[i];
	if(!disjunct_value[k++]) ++rank[i];
	else ++rank[j];
      }
    }
    
    for(i=0; i<m; ++i) {
      order[rank[i]] = i;
    }

    os << " c ";

    for(i=0; i<m; ++i) {
      os << (order[i]+1) << " ";
    }

    //os << std::endl;

    delete [] rank;
    delete [] order;

  } else {
    for(i=0; i<n; ++i) {
      os << std::setw(3) << solver->SearchVars[i].id() ;
    }
    os << std::endl;
    for(i=0; i<n; ++i) {
      os << std::setw(3) << search_value[i] ;
    } 
    os << std::endl;
  }
  return os;
}


// SchedulingSolver::SchedulingSolver(SchedulingSolver* m, 
// 				   ParameterList* p,
// 				   StatisticList* s) 
//   : Solver(), stats(s) 
// { 
//   model = m; 
//   params = p;
//   stats = s;
//   stats->solver = this;

//   params->initialise(this);

//   // stats->lower_bound = model->get_lb();//lower_bound;
//   // stats->upper_bound = model->get_ub();//upper_bound;
  
//   //nogoods = NULL;
//   pool = new SolutionPool();

//   //addHeuristic( params->Heuristic, params->Randomized, params->IValue, params->Hlimit );
// }

// std::ostream& SchedulingSolver::print_weights(std::ostream& os) {
//   int i, n=variables.size;
//   for(i=0; i<n; ++i) {
//     sequence[i]->print(os);
//     os << " " << sequence[i]->weight;
//     os << " " << std::setw(4) << heuristic->get_value(sequence[i]) << std::endl;
//   }
//   //os << std::endl;
//   n=constraints.size;
//   for(i=0; i<n; ++i) {
//     constraints[i]->print(os);
//     os << " " << std::setw(4) << constraints[i]->weight << std::endl;
//   }
//   //os << std::endl;
//   return os;
// }

// void SchedulingSolver::decay_weights(const double decay) {
//   int i, w, n=variables.size;
//   for(i=0; i<n; ++i) {
//     w = (int)(((double)(variables[i]->weight))*decay);
//     if(w > variables[i]->degree) variables[i]->weight = w;
//     else variables[i]->weight = variables[i]->degree;
//   }
//   n = constraints.size;
//   for(i=0; i<n; ++i) {
//     if(constraints[i]->arity < variables.size/2) {
//       w = (int)(((double)(constraints[i]->weight))*decay);
//       if(w > 1) constraints[i]->weight = w;
//       else constraints[i]->weight = 1;
//     }
//   }
// }

// void SchedulingSolver::jtl_presolve() 
// {
//   int objective = stats->upper_bound, new_objective;
  
//   setVerbosity(params->Verbose);

//   setNodeLimit(50000);

//   addHeuristic("osp-t", 1, "anti", 1);
//   JOB Heuristic( model, false );
//   add( Heuristic );

//   solver->set_objective(stats->upper_bound);
//   addObjective();
  
//   std::cout << " c =============[ greedy step for jtl ]=============" << std::endl;
//   std::cout << std::left << std::setw(30) << " c node cutoff" << ":"  
// 	    << std::right << std::setw(20) << 50000 << std::endl;
//   //for(int iteration=0; iteration<params->InitBound; ++iteration) {

//   ((JobByJob*)heuristic)->shuffle();
//   solve_and_restart();
//   //solve();
  
//   if( status == SAT ) {
//     new_objective = solver->get_objective();
    
//     if(objective>new_objective) {
//       objective = new_objective;
//       pool->add(new SchedulingSolution(solver, this));
     
      
//       //std::cout << std::left << std::setw(30) << " c iteration / objective" << ":" << std::right << std::setw(11) << (iteration+1) << " / " << std::setw(6) << objective << ")" << std::endl;
//       std::cout << std::left << std::setw(30) << " c objective" << ":" << std::right << std::setw(20) << objective << std::endl;
//     }
    
//   } else {
//     std::cout << std::left << std::setw(30) << " c jtl presolve " << ":" << std::right << std::setw(20) << "failed" << std::endl;
//     params->InitBound = 0;
//   }
  
//   reset(true);
//   decay_weights(params->Decay);
  
  
//   //exit(1);
//   //}
  
//   std::cout << " c ===============[ end greedy step ]===============" << std::endl;   
//   std::cout << std::endl;
  
//   //resetBacktrackLimit();
//   resetNodeLimit();
//   removeObjective();
  
//   if(pool->size()) {
//     addHeuristic( params->Heuristic, params->Randomized, params->DValue, params->Hlimit );
//   } else {
//     addHeuristic( params->Heuristic, params->Randomized, params->IValue, params->Hlimit );
//   }
  
//   stats->upper_bound = objective;
  
// }

// void SchedulingSolver::old_jtl_presolve() 
// {
//   int objective = stats->upper_bound, new_objective;
  
//   setVerbosity(params->Verbose);

//   setBacktrackLimit(1000);

//   addHeuristic("osp-t", 1, "anti", 1);
//   JOB Heuristic( solver, true );
//   add( Heuristic );

//  //  solver->set_objective(stats->upper_bound);
// //   addObjective();
  
//   std::cout << " c =============[ greedy step for jtl ]=============" << std::endl;
//   std::cout << std::left << std::setw(30) << " c backtrack cutoff" << ":"  
// 	    << std::right << std::setw(20) << 1000 << std::endl;
//   for(int iteration=0; iteration<params->InitBound; ++iteration) {
    
//     ((JobByJob*)heuristic)->shuffle();
//     //solve_and_restart();
//     solve();
    
//     if( status == SAT ) {
//       new_objective = solver->get_objective();
      
//       if(objective>new_objective) {
// 	objective = new_objective;
// 	pool->add(new SchedulingSolution(solver, this));
	
// 	std::cout << std::left << std::setw(30) << " c iteration / objective" << ":" << std::right << std::setw(11) << (iteration+1) << " / " << std::setw(6) << objective << ")" << std::endl;
// 	//std::cout << std::left << std::setw(30) << " c objective" << ":" << std::right << std::setw(20) << objective << std::endl;
//       }
      
//     } else {
//       std::cout << std::left << std::setw(30) << " c jtl presolve " << ":" << std::right << std::setw(20) << "failed" << std::endl;
//       params->InitBound = 0;
//     }
    
//     reset(true);
//     decay_weights(params->Decay);

    
//     //exit(1);
//   }
   
//   std::cout << " c ===============[ end greedy step ]===============" << std::endl;   
//   std::cout << std::endl;
  
//   resetBacktrackLimit();
//   //resetNodeLimit();
//   //removeObjective();
  
//   if(pool->size()) {
//     addHeuristic( params->Heuristic, params->Randomized, params->DValue, params->Hlimit );
//   } else {
//     addHeuristic( params->Heuristic, params->Randomized, params->IValue, params->Hlimit );
//   }
  
//   stats->upper_bound = objective;
  
// }


// bool SchedulingSolver::probe_ub() 
// {
//   bool ret_value = false;
//   int objective = stats->upper_bound;
  
//   setVerbosity(params->Verbose);

//   addHeuristic( params->Heuristic, params->Randomized, params->IValue, params->Hlimit );


//   std::cout << " c =============[ initial probing step ]============" << std::endl;
//   std::cout << std::left << std::setw(30) << " c propag cutoff" << ":"  
// 	    << std::right << std::setw(20) << (params->NodeCutoff/100) << std::endl;
//   setPropagsLimit(params->NodeCutoff/100);

//   status = solver->set_objective(objective);

//   if(status == UNKNOWN) {
//     solve_and_restart(params->PolicyRestart, params->Base, params->Factor);
//   }

//   if( status == SAT ) {
//     ret_value = true;
//     objective = solver->get_objective();
//     stats->normalized_objective = solver->get_normalized_objective();
//     pool->add(new SchedulingSolution(solver, this));
//     stats->upper_bound = objective;
	
//      std::cout << std::left << std::setw(30) << " c solutions's objective" << ":" << std::right << std::setw(20) << objective << std::endl;

//   } else if( status == UNSAT ) {

//     stats->lower_bound = objective+1;
    
//   } else {
//     std::cout << std::left << std::setw(30) << " c probing " << ":" << std::right << std::setw(20) << "failed" << std::endl;
//   }

//   reset(true);
//   decay_weights(params->Decay);

//   if(pool->size()) {
//     addHeuristic( params->Heuristic, params->Randomized, params->DValue, params->Hlimit );
//   } 

//   stats->upper_bound = objective;

//   resetPropagsLimit();
//   resetFailureLimit();

//    std::cout << " c ===============[ end probing step ]==============" << std::endl;

//   return ret_value;
// }

void SchedulingSolver::dichotomic_search(int boostlb)
{
  
  //   presolve();
  
  
  //exit(1);
  
  
  //   // if(!probe_ub()) {
  //   //   if(params->Presolve == "jtl_old") {
  //   //     old_jtl_presolve();
  //   //   } else if(params->Presolve == "jtl") {
  //   //     jtl_presolve();
  //   //   }
  //   // }

	Vector< Vector< Literal > > clauses_kept_between_dicho_steps;


  stats->lower_bound = get_lb();
  stats->upper_bound = get_ub();
//  int current_learnClauses_size = 0;
  int iteration = 1;
  
  int minfsble = stats->lower_bound;
  int maxfsble = stats->upper_bound;
  
  int int_objective = -1;
  int new_objective = -1;
  //   int ngd_stamp = 0;
  //   int lit_stamp = 0;
  
  
  parameters.verbosity = params->Verbose;
  if (boostlb){
	  std::cout << " c Improving LB " << std::endl;
	  parameters.time_limit = params->lbCutoff;
  }
  else
  parameters.time_limit = params->Cutoff;
  
  //   setVerbosity(params->Verbose);
  //   setTimeLimit(params->Cutoff);
  //   if(params->Randomized > 0)
  //     setRandomized(params->Randomized);
  //   else if(params->Randomized < 0) {
  //     randomizeSequence();
  //   }
  
  //   if(level < init_level) presolve();
  
  //   nogoods = NULL;
  //   if(params->Rngd) nogoods = setRestartGenNogood();
  
  
  // BranchingHeuristic *heu = new GenericHeuristic <
  //   GenericWeightedDVO < FailureCountManager, MinDomainOverWeight >,
  //   RandomMinMax 
  //   > (this); 
  BranchingHeuristic *heu=NULL;
  if (params->vsids)
	  heu= new GenericHeuristic< VSIDS<2>, Guided< MinValue > >(this);
  else
	  if (params->taskweight)
	  heu = new SchedulingWeightedDegree < TaskDomOverTaskWeight, Guided< MinValue >, 2 > (this, disjunct_map);
	  else
		  heu = new SchedulingWeightedDegree < TaskDomOverBoolWeight, Guided< MinValue >, 2 > (this, disjunct_map);



  //TODO relate that to VSIDS?
  if (parameters.fd_learning && parameters.forgetfulness)
	  activity_mngr =  new LearningActivityManager(this);

  //BranchingHeuristic *heu = new GenericHeuristic < NoOrder, MinValue > (this);
  RestartPolicy *pol ;
  if (params->PolicyRestart==GEOMETRIC)
	  pol = new Geometric(params->Base,params->Factor);
	//  pol = new Geometric(256,params->Factor);
  else if (params->PolicyRestart==LUBY)
	  pol = new Luby();
  else {
	  std::cout << " RestartPolicy not found " << std::endl;
	  exit(1);
  }

  // RestartPolicy *pol = new Luby();
//  RestartPolicy *pol = new AlwaysRestart();


 // std::cout << std::endl << sequence << std::endl;

  if (params->taskweight && params->vsids){
	  std::cout << "Error params->taskweight && params->vsids "  << std::endl;
	  exit(1);
  }

  initialise_search(disjuncts, heu, pol);
  FailureCountManager* failmngr=NULL;
  _constraint_weight = NULL;
  _variable_weight = NULL;
  if (!params->vsids){
	  if (params->weight_history){
	  if (params->taskweight)
		  failmngr = (FailureCountManager*) (((SchedulingWeightedDegree< TaskDomOverTaskWeight, Guided< MinValue >, 2 >*) heu)-> var.manager);
	  else
		  failmngr = (FailureCountManager*) (((SchedulingWeightedDegree< TaskDomOverBoolWeight, Guided< MinValue >, 2 >*) heu)-> var.manager);

	  _constraint_weight  = new Vector<double> (failmngr->constraint_weight);
	  _variable_weight = new Vector<double> (failmngr->variable_weight);
	  }
  }

  //propagate the bounds, with respect to the initial upper bound
  Outcome result = (IS_OK(propagate()) ? UNKNOWN : UNSAT);

//  std::cout << " Propagated!  " << std::endl;

  //std::cout << " b1491 after propag:  " << variables[1491].get_domain() << std::endl;
  //exit(1);

  if (params->lazy_generation)
  if (base){
	/*  if (!params->forgetall){

		  std::cout << " !forgetall & lazy_generation are not implemented yet! " << std::endl;
		  exit(1);
	  }*/

	  init_lazy_generation();
	  base->extend_vectors(10000);
	  for (int i = 0; i < start_from; ++i)
		  (static_cast<VariableRangeWithLearning*> (variables[i].range_domain))->domainConstraint->extend_vectors();
  }




  init_obj  = (int)(floor(((double)minfsble + (double)maxfsble)/2));
#ifdef _CHECK_NOGOOD
  //int init_obj  = (int)(floor(((double)minfsble + (double)maxfsble)/2));
  int id;
#endif
  ////////// dichotomic search ///////////////
  while( //result == UNKNOWN && 
	 minfsble<maxfsble && 
	 iteration<params->Dichotomy
  ) {

	  parameters.nextforget=parameters.fixedForget;
	  pol->initialise(parameters.restart_limit);

	  double remaining_time = params->Optimise - stats->get_total_time();

	  if(remaining_time < (2*params->NodeBase)) break;

	  if (remaining_time < parameters.time_limit)
		  parameters.time_limit = remaining_time;

	  int_objective = (int)(floor(((double)minfsble + (double)maxfsble)/2));

	  if ((int_objective== minfsble) || (int_objective== maxfsble))
		break;

#ifdef _CHECK_NOGOOD
	  dicho_lb=minfsble;
	  dicho_ub= maxfsble;
#endif
	  std::cout << " c \n c \n c+=========[ start dichotomic step ]=========+" << std::endl;
	  //       setPropagsLimit(params->NodeCutoff);

	  if(boostlb){
//		  parameters.propagation_limit =lbNodeCutoff;
		  parameters.propagation_limit =0;
	  }
	  else
		  parameters.propagation_limit = params->NodeCutoff;

	  std::cout << std::left << std::setw(30) << " c | current real range" << ":"
			  << std::right << " " << std::setw(5) << stats->lower_bound
			  << " to " << std::setw(5) << stats->upper_bound << " |" << std::endl;
	  std::cout << std::left << std::setw(30) << " c | current dichotomic range" << ":"
			  << std::right << " " << std::setw(5) << minfsble
			  << " to " << std::setw(5) << maxfsble << " |" << std::endl;
	  std::cout << std::left << std::setw(30) << " c | target objective" << ":"
			  << std::right << std::setw(15) << int_objective << " |" << std::endl;


	  statistics.start_time = get_run_time();

	  save();


	  result = set_objective(int_objective);


#ifdef _DEBUG_PRUNING
	  monitor(tasks);
	  monitor(C_max);
#endif


	  //std::cout << " nb_clauses size " << base->nb_clauses.size << std::endl;
	  //std::cout << " nb_clauses " << base->nb_clauses << std::endl;
	  result = restart_search(level);

	  //std::cout << " nb_clauses size " << base->nb_clauses.size << std::endl;
	  //std::cout << " nb_clauses " << base->nb_clauses << std::endl;
	 // std::cout << " learnt " << base->learnt << std::endl;
	 // exit(1);
	  if (base)
			  base->static_forget();

	  //Never happened in my tests but we should do that
	  if (limit_generated)
		  params->forgetall=1;

	  if( result == SAT ) {
		  new_objective = get_objective();
		  //   std::cout << " c SAT! " << std::endl;
		  // 	stats->normalized_objective = solver->get_normalized_objective();

		  // at level 0, deduce new bounds for all variables with respect to the new objective
		  set_objective(new_objective);
		  propagate();

		  maxfsble = new_objective;
		  pool->add(new SchedulingSolution(this));

		  // 	if(nogoods) {
		  // 	  for(int i=ngd_stamp; i<nogoods->base->nogood.size; ++i)
		  // 	    stats->avg_nogood_size += (double)(nogoods->base->nogood[i]->size);
		  // 	  if(params->Rngd>1) stats->num_nogoods = nogoods->base->nogood.size;
		  // 	  else stats->num_nogoods += nogoods->base->nogood.size;
		  // 	}

		  std::cout << std::left << std::setw(30) << " c | new upper bound" << ":" << std::right << std::setw(15) << new_objective << " |" << std::endl;
		  if (base && (!params->forgetall) ){

			  clauses_kept_between_dicho_steps.clear();
			  Vector< Literal > tmp;
			  int __size = base->learnt.size ;

			  while (__size--){
				  tmp.clear();
				  Clause & a = *base->learnt[__size];
				  for (int i = (a.size -1); i >= 0; --i)
					  tmp.add(a[i]);
				  clauses_kept_between_dicho_steps.add(tmp);
			  }
		  }

		  //std::cout << " \n c clauses_kept_between_dicho_steps size" << clauses_kept_between_dicho_steps.size << std::endl;
		  //std::cout << "  c base->learnt.size" << base->learnt.size << std::endl;


		  //pool->getBestSolution()->print(std::cout);

		  //Update _constraint_weight and _variable_weight

		  if (!params->vsids){
			  delete _constraint_weight;
			  delete _variable_weight;
			  if (params->weight_history){
//			  failmngr = (FailureCountManager*)  (((SchedulingWeightedDegree< TaskDomOverBoolWeight, Guided< MinValue >, 2 >*) heuristic)->var.manager);
			  if (parameters.taskweight)
				  failmngr = (FailureCountManager*) (((SchedulingWeightedDegree< TaskDomOverTaskWeight, Guided< MinValue >, 2 >*) heu)-> var.manager);
			  else
				  failmngr = (FailureCountManager*) (((SchedulingWeightedDegree< TaskDomOverBoolWeight, Guided< MinValue >, 2 >*) heu)-> var.manager);

			  _constraint_weight  = new Vector<double> (failmngr->constraint_weight);
			  _variable_weight = new Vector<double> (failmngr->variable_weight);
		  }
		  }

	  } else {
		  //s    	std::cout << " c NOT SAT! " ;
		  new_objective = int_objective;


		  if( result == UNSAT ) {
			  minfsble = int_objective+1;

			  //	  std::cout << " UNSAT! " ;
			  std::cout << std::left << std::setw(30) << " c | real lower bound" << ":" << std::right << std::setw(15) << minfsble << " |" << std::endl;
		  } else {

			  if(boostlb){
				  maxfsble = int_objective  ;
				  std::cout << std::left << std::setw(30) << " c | dichotomic upper bound" << ":" << std::right << std::setw(15) << maxfsble << " |" << std::endl;

			  }
			  else
			  {
				  minfsble = int_objective+1;

				  std::cout << std::left << std::setw(30) << " c | dichotomic lower bound" << ":" << std::right << std::setw(15) << minfsble << " |" << std::endl;
			  }
		  }

		  //	  std::cout << std::endl;


		  // 	if(nogoods) {
		  // 	  nogoods->forget(ngd_stamp);
		  // 	  nogoods->reinit();
		  // 	}
	  }

	  stats->add_info(new_objective, DICHO);

	  std::cout << std::left << std::setw(30) << " c | cpu time" << ":" << std::right << std::setw(15) << (double)((int)((stats->time.back())*10000))/10000.0 << " |" << std::endl;

	  //printStatistics(std::cout, ((params->Verbose ? RUNTIME : 0) + ((params->Verbose || result != UNKNOWN)  ? BTS + PPGS : 0) + OUTCOME) );


	  //std::cout << "LEVEL: " << level << " " << this << std::endl;

	  restore();

	  if (base)
	  {
		  std::cout << std::left << std::setw(30) << " c | nb variables " << ":" << std::right << std::setw(15) << variables.size<< " |" << std::endl;
		  std::cout << std::left << std::setw(30) << " c | learnt clauses " << ":" << std::right << std::setw(15) << base->learnt.size << " |" << std::endl;
		  std::cout << std::left << std::setw(30) << " c | avg nogood size " << ":" << std::right << std::setw(15) <<(int)statistics.avg_learned_size<< " |" << std::endl;
#ifdef _CHECK_NOGOOD
		  std::cout << std::left << std::setw(30) << " c | All nogoods are correct " << std::right << std::setw(15) <<" |" << std::endl;
#endif
		  /* Forgetting Clauses :
		   * If the flag forgetAll is set to 1 then forget all clauses betweeen all dichotomy iterations
		   * Otehrwise, forget the newly learnt clauses iff. the result of the current dichotomy is UNSAT
		   */

		  start_over(false, false, params->forgetall);
		  if (!params->forgetall)
		  {
			  if (parameters.lazy_generation){
				  relaxed_variables.clear();
				  for (int i = 0; i < start_from; ++i)
					  (static_cast<VariableRangeWithLearning*> (variables[i].range_domain))->domainConstraint->reset_locked();
			  }
			  for (int i = (clauses_kept_between_dicho_steps.size-1) ; i>= 0; --i){
				  base->learn(clauses_kept_between_dicho_steps[i], (parameters.init_activity ? parameters.activity_increment : 0.0));
				  base->learnt.back()->locked = false;
			  }
			  base->unlocked_clauses+= clauses_kept_between_dicho_steps.size;
			  relax_generated_variables();
			 //if (parameters.lazy_generation)
			  // relax_generated_variables();
		  }
		  std::cout << " c forget all clauses between dicho steps ?: " << (params->forgetall? "yes" : "no") << " i.e. the database size is now " << base->learnt.size << std::endl;
	  }

	  statistics.initialise(this);
	 //Moved Up!
	  //pol->initialise(parameters.restart_limit);

	  // std::cout << std::left << std::setw(30) << " c current dichotomic range" << ":"
	  // 	      << std::right << std::setw(6) << " " << std::setw(5) << minfsble
	  // 	      << " to " << std::setw(5) << maxfsble << " " << iteration << " " << params->Dichotomy << std::endl;
	  std::cout << " c +==========[ end dichotomic step ]==========+" << std::endl;
	//  std::cout << " \n end step " << std::endl;
  	 //exit(1);

	  // A module for checking learnt nogoods
#ifdef _CHECK_NOGOOD
	  nogood_origin.clear();
	  nogood_clause.clear();
	  __nogoods.clear();
	  solution.clear();
	  node_num.clear();
	  atom.clear();
#endif


	//  std::cout << " SLEEP FOR 10s " << std::endl;
	//  std::this_thread::sleep_for (std::chrono::seconds(10));
	  //exit(1);

	  initialise_heuristic(params->weight_history & 2);

	  ++iteration;

#ifdef _DEBUG_SCHEDULER
	  return;
#endif
  } 
  //   } else if( status == SAT ) {
  //     std::cout << " c Solved during preprocessing!" << std::endl;
  
  //   } else if( status == UNSAT ) {
  //     std::cout << " c Found inconsistent during preprocessing!" << std::endl;
  
  //   }
    
  //std::cout << std::endl;
  delete objective;
  objective=NULL;
}
 

// void SchedulingSolver::all_solutions_search()
// {

//   SolutionPool *second_pool = new SolutionPool();

//   presolve();
  
//   Vector< Decision > literals;
//   int iteration = 0;
//   int minfsble = stats->lower_bound;
//   int maxfsble = stats->upper_bound;

//   setVerbosity(params->Verbose);
//   setTimeLimit(params->Cutoff);
//   if(params->Randomized > 0)
//     setRandomized(params->Randomized);
//   else if(params->Randomized < 0) {
//     randomizeSequence();
//   }

//   nogoods = setRestartGenNogood();

//   while(true) {

//     int ds_iteration = 1;

//     int objective = -1;
//     int new_objective = -1;
//     int ngd_stamp = 0;
//     int lit_stamp = 0;
    
//     bool solution_found = false;

//     if(params->Verbose>=0)
//       std::cout << " c ============[ start dichotomic step ]============" << std::endl;

//     ////////// dichotomic search ///////////////
//     if(status == UNKNOWN) {
//       while( minfsble<maxfsble && 
// 	     ds_iteration<params->Dichotomy
// 	     ) {
	
// 	double remaining_time = params->Optimise - stats->get_total_time();
	
// 	if(remaining_time < (2*params->NodeBase)) break;

// 	objective = (int)(floor(((double)minfsble + (double)maxfsble)/2));

// 	setPropagsLimit(params->NodeCutoff);

// 	if(params->Verbose>=0) {
// 	  std::cout << std::left << std::setw(30) << " c current dichotomic range" << ":" 
// 		  << std::right << std::setw(6) << " " << std::setw(5) << minfsble 
// 		    << " to " << std::setw(5) << maxfsble << std::endl;
// 	  std::cout << std::left << std::setw(30) << " c target objective" << ":"  
// 		    << std::right << std::setw(20) << objective << std::endl;
// 	}

// 	save();
	
// 	status = solver->set_objective(objective);
// 	if(pool->size()) {
// 	  if(params->DValue == "guided") {

// 	    //std::cout << params->Type << std::endl;

// // 	    if(params->Type == "now2")
// // 	      pool->getBestSolution()->guide_search_bounds();
// // 	    else
// 	      pool->getBestSolution()->guide_search();
// 	  }
// 	}
	
// 	ngd_stamp = (params->Rngd>1 ? nogoods->base->nogood.size : 0);
// 	lit_stamp = sUnaryCons.size;
	
// 	if(status == UNKNOWN) {
	  
// // 	  std::cout << "nogood base:" << std::endl;
// // 	  nogoods->print(std::cout);

// 	  for(int i=0; i<literals.size; ++i) {
// 	    sUnaryCons.add(literals[i]);
// 	  }

// 	  solve_and_restart(params->PolicyRestart, params->Base, params->Factor);
// 	}
	
// 	nogoods->forget(ngd_stamp);
// 	nogoods->reinit();
	
// 	if( status == SAT ) {

// 	  //std::cout << " c makespan " << solver->C_max.value() << std::endl;

// 	  solution_found = true;
// 	  new_objective = solver->get_objective();
	  
// 	  stats->normalized_objective = solver->get_normalized_objective();
	  
// 	  maxfsble = new_objective;
// 	  pool->add(new SchedulingSolution(solver, this));
	  
// 	  if(params->Verbose>=0) {
// 	    std::cout << std::left << std::setw(30) << " c solutions's objective" << ":" << std::right << std::setw(20) << new_objective << std::endl;
// 	  }

// 	} else {
	  
// 	  new_objective = objective;
// 	  minfsble = objective+1;
	  
// 	}

// 	stats->add_info(new_objective, DICHO);
	
// 	//printStatistics(std::cout, ((params->Verbose ? RUNTIME : 0) + ((params->Verbose || status != UNKNOWN)  ? BTS + PPGS : 0) + OUTCOME) );
	
	
// 	reset(true);
// 	decay_weights(params->Decay);

// 	if(pool->size() && (params->DValue != params->IValue)) {
// 	  addHeuristic( params->Heuristic, params->Randomized, params->DValue, params->Hlimit );
// 	}
      
      
// 	++ds_iteration;
//       } 
//     } else if( status == SAT ) {
//       std::cout << " c Solved during preprocessing!" << std::endl;
      
//     } else if( status == UNSAT ) {
//       std::cout << " c Found inconsistent during preprocessing!" << std::endl;
      
//     }
//     if(params->Verbose>=0) {
//       std::cout << " c =============[ end dichotomic step ]=============" << std::endl;
//     }

//     //std::cout << status << " =?= " << UNKNOWN << std::endl;

//     if(!solution_found) break;

//     // std::cout << maxfsble << ": ";
//     //  pool->getBestSolution()->print(std::cout, "fsp");
//     //  std::cout << " ";
//     // pool->getBestSolution()->print(std::cout, "jsp");
//     // std::cout << std::endl;
//     // // add the nogood corresponding to that solution

//     second_pool->add(pool->getBestSolution());

//     //std::cout << maxfsble << std::endl;

//     if(maxfsble>0) {

//       //int choice = params->NgdType;
//       //if(choice == 2) choice = (maxfsble<=(solver->disjuncts.size/4));
      
//       //for(int k=0; k<2; ++k) {
// 	Vector< Decision > learnt;
// 	int k=1;
// 	for(int i=0; i<solver->disjuncts.size/2; ++i) {
// 	  if(pool->getBestSolution()->disjunct_value[i] == k) {
// 	    Decision d('n', k, solver->disjuncts[i].getVariable());
// 	    learnt.add(d);
// 	    // 	  d.print(std::cout);
// 	    // 	  std::cout << " ";
// 	  }
// 	}
// 	//std::cout << std::endl;
// 	if(learnt.size>1)
// 	  nogoods->base->add(learnt);
// 	else
// 	  literals.add(learnt[0]);
// 	//}

//       //learnt[0].propagate();
//       stats->upper_bound = maxfsble = solver->disjuncts.size;

//       //exit(1);
//     }

//     ++iteration;

//     if(maxfsble == 0) break;
//     //if(iteration>2)
//     //break;
//   }
      
//   stats->num_solutions = iteration;
//   std::cout << "d ITERATIONS  " << iteration << std::endl;


// //   for(int i=0; i<second_pool->size(); ++i) {
// //     for(int j=i+1; j<second_pool->size(); ++j) {
// //       std::cout << "dist=" << (*second_pool)[i]->distance((*second_pool)[j]) << " (";
// //       (*second_pool)[i]->print(std::cout, "fsp");
// //       std::cout << " / " ;
// //       (*second_pool)[j]->print(std::cout, "fsp");
// //       std::cout << ")" << std::endl ;

// //     }
// //   }

// }


// // void SolutionGuidedSearch::execute() 
// //   { 
// //     SchedulingSolver *ss = (SchedulingSolver*)solver;
// //     pool->add(new SchedulingSolution(ss->solver, ss));
// //     if(pool->size()) pool->getBestSolution()->guide_search();
// //     StoreStats::execute();
// //   }


// int SchedulingSolver::virtual_iterative_dfs()
//     {
//       SimpleUnaryConstraint last_decision;
      
//       while( status == UNKNOWN ) {
	
// 	if( filtering() ) {
// 	  if(verbosity>2) {
// 	    VariableInt *t;
// 	    for(int i=0; i<solver->data->nJobs(); ++i) {
// 	      std::cout << "j" << std::left << std::setw(2) << i << std::right ; //<< " ";
// 	      //std::cout << " (" << solver->data->nTasksInJob(i) << ") ";
// 	      //std::cout.flush();

// 	      //if(params->Type != "now" && params->Type != "now2") {
// 	      if(solver->tasks.size > solver->data->nJobs()) {
// 		for(int j=0; j<solver->data->nTasksInJob(i); ++j) {
// 		  t = solver->tasks[solver->data->getJobTask(i,j)].getVariable();
// 		  std::cout << " " << t->id+1 << "[" << t->min() << ".." << t->max() << "]" ;
// 		}
// 	      } else {
// 		t = solver->tasks[i].getVariable();
// 		std::cout << " " << t->id+1 << "[" << t->min() << ".." << t->max() << "]" ;
// 	      }
// 	      std::cout << std::endl;
// 	    }
// 	  }

// 	  if( future == empty ) {
// 	    solutionFound(init_level);
// 	  } else {

// 	    newNode();
	   
// 	    if(verbosity>2) {
// 	      VariableInt *d = decision.back();
// 	      MistralNode<Constraint*> *nd = d->constraintsOnValue();
	      
// 	      while( nextNode(nd) ) 
// 		if(nd->elt->arity == 3) {
// 		  PredicateDisjunctive *p = (PredicateDisjunctive *)(nd->elt);
// 		  //std::cout << "HERE: " << p;
// 		  std::cout << " c";
// 		  for(int k=0; k<=level; ++k) std::cout << " ";
// 		  p->print(std::cout);
// 		  std::cout << std::endl;
// 		}
// 	    }

// 	  }
// 	} else {
// 	  if( level <= init_level ) {
	    
// #ifdef _DEBUGSEARCH
// 	    if(verbosity > 2) {
// 	      std::cout << " c UNSAT!" << std::endl;
// 	    }
// #endif
	    
// 	    status = UNSAT;
// 	  } else if( limitsExpired() ) {
	    
// #ifdef _DEBUGSEARCH
// 	    if(verbosity > 2) {
// 	      std::cout << " c";
// 	      for(int k=0; k<=level; ++k) std::cout << " ";
// 	      SimpleUnaryConstraint d = branching_decision[level];
// 	      d.revert();
// 	      d.print(std::cout);
// 	      std::cout << " (limit expired at level " << level << ")" << std::endl;
// 	    }
// #endif
	    
// 	    status = LIMITOUT;
// 	  } else {
// 	    last_decision = branching_decision[level];

// #ifdef _DEBUGSEARCH
// 	    if(verbosity > 2) {
// 	      if( level > backtrackLevel+1 ) {
// 		std::cout << " c";
// 		for(int k=0; k<=level; ++k) std::cout << " ";
// 		std::cout << " backjump to level " << backtrackLevel << std::endl;
// 	      }
// 	    }
// #endif
	    
// 	    backtrackTo( backtrackLevel );	    
// 	    last_decision.deduce();

// #ifdef _DEBUGSEARCH
// 	    if(verbosity > 2) {
// 	      std::cout << " c";
// 	      for(int k=0; k<=level; ++k) std::cout << " ";
// 	      last_decision.print( std::cout );
// 	      std::cout << std::endl;
// 	    }
// #endif

// 	  }
// 	}
//       }
//       return status;
//     }

void SchedulingSolver::branch_and_bound()
{
stats->num_solutions++;

  //int ngd_stamp = 0;
  //int lit_stamp = 0;
  //resetNodeLimit();
  //resetPropagsLimit();
  parameters.propagation_limit = 0;

  statistics.start_time = get_run_time();

  //std::cout << (get_run_time() - statistics.start_time) << std::endl;
  //int old_learning = parameters.fd_learning;
  //Cancel learning : check this when alowing lazy generation


  if (base){
	  if (!parameters.keeplearning_in_bb){
		  start_over(false ,false, true);
		  parameters.fixedForget=0;
		  parameters.fd_learning = 0;
		  parameters.backjump = 0;
		  delete[] visitedUpperBoundvalues;
		  delete[] visitedLowerBoundvalues;

		  delete[] visitedLowerBoundExplanations;
		  delete[] visitedUpperBoundExplanations;

		  delete[] visitedLowerBoundlevels;
		  delete[] visitedUpperBoundlevels;

		  delete[] visitedLowerBoundorders;
		  delete[] visitedUpperBoundorders;

		  base->enforce_nfc1 = true;
		  base->relax();
		  if (parameters.lazy_generation){
			  VariableRangeWithLearning* tmp_VariableRangeWithLearning;
			  DomainFaithfulnessConstraint* dom_constraint ;
			  for (int var = 0; var< start_from; ++var){
				  tmp_VariableRangeWithLearning =static_cast<VariableRangeWithLearning*>(variables[var].range_domain);
				  dom_constraint = tmp_VariableRangeWithLearning->domainConstraint;
				  //dom_constraint->enforce_nfc1 = true;
				  dom_constraint->enforce_nfc1 = true;
				  dom_constraint->relax();
			  }
			  parameters.lazy_generation = 0;
		  }
	  }
	  else{
		  if (params->forgetall)
		  {
			  /*std::cout << " c  base->learnt.size=" << base->learnt.size << std::endl;
				  std::cout << " c  base->will_be_forgotten.size=" << base->will_be_forgotten.size << std::endl;
				  std::cout << " c unlocked clause : " << base->unlocked_clauses << std::endl;
			   */
			  start_over(false,  false, true);
		  }
		  //parameters.forgetfulness = params->BBforgetfulness;
		  parameters.nextforget=parameters.fixedForget;
	  }
  }


  if (policy)
	  delete policy ;

  if (params->BandBPolicyRestart==GEOMETRIC)
	  policy = new Geometric(params->Base,params->Factor);
  else if (params->BandBPolicyRestart==LUBY)
	  policy = new Luby();
  else {
	  std::cout << " RestartPolicy not found " << std::endl;
	  exit(1);
  }

  //In order to simulate initialise_search() we need these two lines
  //parameters.restart_limit = policy->base;
  parameters.limit = (policy->base > 0);
  policy->initialise(parameters.restart_limit);


/*  std::cout << "restart_limit " <<  parameters.restart_limit << std::endl;
  std::cout << "num_failures " <<  statistics.num_failures << std::endl;
  std::cout << "policy base " << policy->base << std::endl;
  std::cout << "policy increment" << ((Geometric *) policy)->increment << std::endl;
*/

//  else {
//	  std::cout << " RestartPolicy not found " << std::endl;
//	  exit(1);
//  }


  initialise_heuristic(params->weight_history & 1);

  if (_constraint_weight)
	  delete _constraint_weight;

  if (_variable_weight)
	  delete _variable_weight;


  save();
  set_objective(stats->upper_bound-1);
  addObjective();

  //  std::cout << " c BB base->learnt.size=" << base->learnt.size << std::endl;
  //  std::cout << " c base->will_be_forgotten.size=" << base->will_be_forgotten.size << std::endl;
  //  std::cout << " c unlocked clause : " << base->unlocked_clauses << std::endl;

  //std::cout << (get_run_time() - statistics.start_time) << std::endl;

  parameters.verbosity = 2;
  //setVerbosity(params->Verbose);
  //setRandomSeed( params->Seed );

  double time_limit = (params->Optimise - stats->get_total_time());

  if(time_limit > 0) {
    set_time_limit( time_limit ); 
    // //addHeuristic( params->Heuristic, params->Randomized, params->Value, params->Hlimit );
    // if(params->Value == "guided") {
    //   //function = new SolutionGuidedSearch( this, pool, stats );
    //   if(pool->size()) pool->getBestSolution()->guide_search();
    // } else {
    //   //function = new StoreStats( this, stats );
    // }
    
    std::cout << " c\n c +=========[ start branch & bound ]==========+" << std::endl;
    std::cout << std::left << std::setw(26) << " c | current range" << ":" 
	      << std::right << std::setw(5) << " " << std::setw(5) << stats->lower_bound 
	      << " to " << std::setw(5) << objective->upper_bound << " |" << std::endl;
    std::cout << std::left << std::setw(26) << " c | run for " << ":"
	      << std::right << std::setw(18) << (time_limit) << "s |" << std::endl;
    

    //std::cout << (get_run_time() - statistics.start_time) << std::endl;
    //std::cout << C_max << " in " << C_max.get_domain() << std::endl;

    //std::cout << this << std::endl;

    //std::cout << level << std::endl;

    

    
    Outcome result = (stats->upper_bound >= stats->lower_bound ? UNKNOWN : UNSAT);

    if(result == UNKNOWN) {

      // if(nogoods) {
      // 	ngd_stamp = nogoods->base->nogood.size;
      // 	lit_stamp = sUnaryCons.size;
      // }

      //solve_and_restart(params->PolicyRestart, params->Base, params->Factor);

      //std::cout << (get_run_time() - statistics.start_time) << std::endl;

      result = restart_search(level);

      // if(nogoods) {
      // 	for(int i=ngd_stamp; i<nogoods->base->nogood.size; ++i)
      // 	  stats->avg_nogood_size += (double)(nogoods->base->nogood[i]->size);
      // 	if(params->Rngd>1) stats->num_nogoods = nogoods->base->nogood.size;
      // 	else stats->num_nogoods += nogoods->base->nogood.size;
      // }
    }
    //std::cout << " avg_learned_size " << statistics.avg_learned_size <<  std::endl;
    //std::cout << " avg_clauses_size  " << stats->avg_clauses_size <<  std::endl;
    stats->add_info(objective->upper_bound, BNB);

    //std::cout << " avg_clauses_size  " << stats->avg_clauses_size<<  std::endl;
    //printStatistics(std::cout, ((params->Verbose ? RUNTIME : 0) + (params->Verbose ? BTS + PPGS : 0) + OUTCOME) );

    //reset(true);
    std::cout << " c +==========[ end branch & bound ]===========+" << std::endl;

  }
}

// void StoreStats::execute()
// { 
//   stats->normalized_objective = ss->solver->get_normalized_objective();
// }  

//Vector<VariableInt*> unstable;

// void SchedulingSolver::extract_stable(List& neighbors, 
// 				      Vector<VariableInt*>& stable)
// {
//   neighbors.clear();
//   neighbors.random_fill(params->Neighbor);

//   stable.clear();
//   unstable.clear();
//   int i, j, k=0, n=neighbors.capacity, m=0;
  
//   for(i=0; i<n; ++i) if(!neighbors.member(i)) {
//       m = solver->data->nTasksInMachine(i);
//       for(j=0; j<(m*(m-1)/2); ++j) {
// 	stable.add(solver->disjuncts[k++].getVariable());
//       }
//     } else {
//       k += (m*(m-1)/2);
// //       m = solver->data->nTasksInMachine(i);
// //       for(j=0; j<(m*(m-1)/2); ++j) {
// // 	unstable.add(solver->disjuncts[k++].getVariable());
// //       }
//     }
// }

// void SchedulingSolver::large_neighborhood_search() {
//    Vector< VariableInt* > stable;
//    List neighbors;
//    neighbors.init(0, solver->data->nMachines()-1);
//    int objective = INFTY;

//    save();
//    solver->set_objective(stats->upper_bound);
//    addObjective();

//    while(true) {
//      // select a subset of variables that will stay stable
//      extract_stable(neighbors,stable);
    
//      // optimise the remaining part
//      objective = stats->upper_bound;
//      repair(pool->getBestSolution(), stable);
//      if(stats->upper_bound <= objective)
//        pool->add(new SchedulingSolution(solver, this));
//      reset(true);
//    }
// }


// void SchedulingSolver::repair(SchedulingSolution *sol, Vector<VariableInt*>& stable)  
// {
//   save();
//   for(int i=0; i<stable.size; ++i) {
//     Decision branch('e', (*sol)[stable[i]], stable[i]);
//     branch.propagate();
//   }
//   solve_and_restart() ;
//   stats->add_info(goal->upper_bound, LNS);
// }


void SchedulingSolver::print_solution(std::ostream& os, std::string type)
{
  pool->getBestSolution()->print(os, type);
  os << std::endl;
}

#ifdef _CHECK_NOGOOD
//For each single nogood we create a new solver with exactly the same parameters!
void SchedulingSolver::check_nogood(Vector<Literal> & c){
	StatisticList __stats;
	__stats.start();
	Instance __jsp(*params);
	//	std::cout << std::endl;
	//	__jsp.printStats(std::cout);

	int old_fd_learning = params->FD_learning;
	int old_keeplearning_in_bb = params->keeplearning_in_bb;
	int old_nogood_based_weight = params->nogood_based_weight;

	params->FD_learning = 0;
	params->keeplearning_in_bb = 0;

	params->nogood_based_weight = 0;

	int lb = params->LBinit ;
	int ub = params->UBinit ;


	//solver->parameters.backjump = false;
	//	std::cout << " \n\n c Lower Bound  " << stats->lower_bound ;
	//	std::cout << " c Upper Bound  " << stats->upper_bound ;
	int obj  = (int)(floor(((double) dicho_lb  + (double) dicho_ub)/2));
	//	std::cout << " c obj  " << stats->upper_bound ;

	params->LBinit = dicho_lb ;
	//params->UBinit = obj;
	params->UBinit =  dicho_ub;
	//	params->print(std::cout);
	SchedulingSolver *__solver;
	if(params->Objective == "makespan") {
		//	std::cout << "c Minimising Makespan" << std::endl;
		if(params->Type == "now") __solver = new No_wait_Model(__jsp, params, -1, 0);
		else if(params->Type == "now2") {
			//params.Type = "now";
			__solver = new No_wait_Model(__jsp, params, -1, 1);
		}
		else __solver = new C_max_Model(&__jsp, params, &__stats);
	} else if(params->Objective == "tardiness") {
		//		std::cout << "c Minimising Tardiness" << std::endl;
		__solver = new L_sum_Model(__jsp, params, -1);
	}
	else {
		std::cout << "c unknown objective, exiting" << std::endl;
		exit(1);
	}
	__solver->consolidate();
	//__solver->save();
//	std::cout << "c obj" << obj <<std::endl;
//	std::cout << "c init_obj" << init_obj <<std::endl;
	__solver->set_objective(obj);

	//std::cout << " check learnt nogood :  "<< c << std::endl;

	// c[0] before all others!
	int id =get_id_boolean_variable(c[0]);
	//		std::cout << " id = " << id << std::endl;
	//		std::cout << " the variable is : " << variables[id] <<" and its domain is : " <<  variables[id].get_domain() << std::endl;
	if (id < initial_variablesize){

		__solver->variables[id].set_domain(SIGN(NOT(c[0])));
	//	std::cout << " literal associated to :  " << variables[id] << " = " << variables[id].get_domain() <<  std::endl;

	}
	else
		if (id < variables.size){

			int id_range = 	varsIds_lazy[id - initial_variablesize];
			int val_range = value_lazy[id - initial_variablesize];

			//	std::cout << " OK : id_range = " << id_range << std::endl;
			//	std::cout << " OK : val_range = " << val_range << std::endl;

			if (SIGN(NOT(c[0]))){
				__solver->variables[id_range].set_max(val_range);
		//		std::cout << " Bound literal associated to :  " << variables[id_range] << " <=  " << val_range <<  std::endl;

			}
			else{
				__solver->variables[id_range].set_min(val_range+1);
		//		std::cout << " Bound literal associated to :  " << variables[id_range] << " >=  " << val_range+1 <<  std::endl;
			}


		}
		else
		{
			std::cout << " ERROR : NOT (i < variables.size " << std::endl;
			exit(1);
		}

	if (assignment_level[id] != level){

		std::cout << " ERROR : assignment_level[id] != level" << std::endl;
		exit(1);
	}

	int bts = search_root;
	for(int j=1; j<c.size; ++j) {
		if (is_a_bound_literal(c[j])){

			int id_range = 	get_variable_from_literal(c[j]);
			int val_range =	get_value_from_literal(c[j]);
			bool isub =is_upper_bound(c[j]);
			VariableRangeWithLearning* tmp_VariableRangeWithLearning =static_cast<VariableRangeWithLearning*>(variables[id_range].range_domain);
			//int lvl = tmp_VariableRangeWithLearning->level_of(val_range,!isub) ;
			int lvl , ass_lvl;
			 bool __t;
			tmp_VariableRangeWithLearning->get_informations_of( val_range , !isub, lvl, ass_lvl, __t, false);

			if (bts < lvl){
				bts = lvl;
			}


			//	std::cout << " OK : id_range = " << id_range << std::endl;
			//	std::cout << " OK : val_range = " << val_range << std::endl;

			if (isub){
				__solver->variables[id_range].set_max(val_range);
	//			std::cout << " Bound literal associated to :  " << variables[id_range] << " <=  " << val_range <<  std::endl;

			}
			else{
				__solver->variables[id_range].set_min(val_range);
	//			std::cout << " Bound literal associated to :  " << variables[id_range] << " >=  " << val_range+1 <<  std::endl;
			}
		}
		else{

		int id =get_id_boolean_variable(c[j]);

		if (bts < assignment_level[id]){
			bts = assignment_level[id];
		}

	//	std::cout << " j = " << j << std::endl;
	//	std::cout << " id = " << id << std::endl;
		//		std::cout << " the variable is : " << variables[id] <<" and its domain is : " <<  variables[id].get_domain() << std::endl;
		if (id < initial_variablesize){

			__solver->variables[id].set_domain(SIGN(NOT(c[j])));
		//	std::cout << " literal associated to :  " << variables[id] << " = " << variables[id].get_domain() <<  std::endl;

		}
		else
			if (id < variables.size){

				int id_range = 	varsIds_lazy[id - initial_variablesize];
				int val_range = value_lazy[id - initial_variablesize];

				//	std::cout << " OK : id_range = " << id_range << std::endl;
				//	std::cout << " OK : val_range = " << val_range << std::endl;

				if (SIGN(NOT(c[j]))){
					__solver->variables[id_range].set_max(val_range);
		//			std::cout << " Bound literal associated to :  " << variables[id_range] << " <=  " << val_range <<  std::endl;

				}
				else{
					__solver->variables[id_range].set_min(val_range+1);
		//			std::cout << " Bound literal associated to :  " << variables[id_range] << " >=  " << val_range+1 <<  std::endl;
				}


			}
			else
			{
				std::cout << " ERROR : NOT (i < variables.size " << std::endl;
				exit(1);
			}



		if (assignment_level[id] >= level){

			std::cout << " ERROR : assignment_level[id] >= level" << std::endl;
			std::cout << " assignment_level[id]" << assignment_level[id] << std::endl;
			std::cout << " assignment_level[id]" << level<< std::endl;
			exit(1);
		}


		//		std::cout << " the new domain is : " << __solver->variables[id].get_domain() << " because its literal is " <<  nogood_clause[i][j] <<std::endl;
	}
	}


	if (bts != backtrack_level){

		std::cout << " ERROR : backtrack_level is not correct! " << std::endl;
		std::cout << " clause :  " << c << std::endl;
		std::cout << " bts :  " << bts << std::endl;
		std::cout << " search root  :  " << search_root << std::endl;
		std::cout << " level   :  " << level << std::endl;
		std::cout << " backtrack_level :  " << backtrack_level << std::endl;

		exit(1);
	}


	//	->chronological_dfs();
	//Outcome  __result= __solver->depth_first_search();

	BranchingHeuristic *__heu = new SchedulingWeightedDegree < TaskDomOverBoolWeight, Guided< MinValue >, 2 > (__solver, __solver->disjunct_map);

	//BranchingHeuristic *heu = new GenericHeuristic < NoOrder, MinValue > (this);

	RestartPolicy *__pol = new Geometric();


	__solver->initialise_search(__solver->disjuncts, __heu, __pol);


	//    std::cout << std::left << std::setw(30) << " c | current real range" << ":"
	//	      << std::right << " " << std::setw(5) << __stats.lower_bound
	//	      << " to " << std::setw(5) << __stats.upper_bound << " |" << std::endl;
	//std::cout << std::left << std::setw(30) << " c | current dichotomic range" << ":"
	//    << std::right << " " << std::setw(5) << minfsble
	//     << " to " << std::setw(5) << maxfsble << " |" << std::endl;
	//std::cout << std::left << std::setw(30) << " c | target objective" << ":"
	// << std::right << std::setw(15) << objective << " |" << std::endl;


	Outcome  __result= __solver->restart_search(0);

	//	cout << s.statistics << endl;
	if(__result)
	{
		std::cout << " c WRONG NOGOOD!!\n";
		//		params->LBinit = stats->lower_bound ;
		//		params->UBinit = obj;

		std::cout << " Lower Bound  " << stats->lower_bound;
		std::cout << " Upper Bound  " << obj ;
		std::cout << "Solver level " << level ;
		std::cout << "clause size " << c.size ;
		std::cout << "clause " << c ;
		//std::cout << "and bounds to explore " << literals_to_explore ;
				std::cout << std::endl;

		//		std::cout << " Lower Bound  " << __stats.lower_bound ;
		//		std::cout << " Upper Bound  " << __stats.upper_bound ;
		exit(1);
	}
	else
//		std::cout << " Is a valid nogood !\n" << std::endl;

	//std::cout << " 1 ";
	//delete __pol;
	//delete __heu;
	delete __solver;

	params->FD_learning = old_fd_learning;
	params->keeplearning_in_bb = old_keeplearning_in_bb;
	params->nogood_based_weight = old_nogood_based_weight;

	if (! parameters.backjump){
		std::cout << " !backjump " << std::endl;
		exit(1);
	}
	params->LBinit = lb;
	params->UBinit = ub ;
	/*if(__solver->propagate()) {
		std::cout << " WRONG NOGOOD!!\n";
		exit(1);
	}
	std::cout << " is a valid nogood !\n";
	delete __solver;
	 */
}
#endif
