/* Author: Fereshte */
#pragma once
#include <angel/utils/stopwatch.hpp>
#include <cplusplus/cuddObj.hh>
#include <cudd/cudd.h>
#include <cudd/cuddInt.h>
#include <fstream>
#include <map>
#include <tweedledum/algorithms/synthesis/linear_synth.hpp>
#include <tweedledum/gates/gate_base.hpp>
#include <tweedledum/gates/gate_lib.hpp>
#include <tweedledum/gates/io3_gate.hpp>
#include <tweedledum/gates/mcmt_gate.hpp>
#include <tweedledum/networks/io_id.hpp>
#include <unordered_set>
#include "utils.hpp"
#include <kitty/kitty.hpp>
#include <angel/utils/helper_functions.hpp>
namespace angel
{

struct qsp_add_statistics
{
  double time{0};
  uint32_t cnots{0};
  uint32_t sqgs{0};
  uint32_t nodes{0};
  uint32_t MC_gates{0};
  uint32_t ancillaes{0};
  void report( std::ostream& os = std::cout ) const
  {
    os << "[i] time: " << time << std::endl;
    os << "[i] nodes: " << nodes << std::endl;
    os << "[i] MC_gates: " << MC_gates << std::endl;
    os << "[i] cnots: " << cnots << std::endl;
    os << "[i] sqgs: " << sqgs << std::endl;
    os << "[i] ancillaes: " << ancillaes << std::endl;
  }
};

struct create_bdd_param
{
  enum class strategy : uint32_t
  {
    create_from_tt,
    create_from_pla
  } strategy = strategy::create_from_tt;
};

namespace detail
{
BDD create_bdd_from_pla( Cudd& cudd, std::string file_name, uint32_t& num_inputs )
{
  std::ifstream infile( file_name );
  std::string in, out;
  infile >> in >> out;
  num_inputs = std::atoi( out.c_str() );
  BDD output;
  auto bddNodes = new BDD[num_inputs];
  for ( int i = num_inputs - 1; i >= 0; i-- )
  {
    bddNodes[i] = cudd.bddVar(); // index 0: LSB
  }

  bool sig = 1;
  while ( infile >> in >> out )
  {
    BDD tmp, var;
    bool sig2 = 1;

    for ( auto i = 0u; i < num_inputs; ++i )
    {
      if ( in[i] == '-' )
        continue;
      var = bddNodes[i];

      if ( in[i] == '0' )
        var = !var;
      if ( sig2 )
      {
        tmp = var;
        sig2 = 0;
      }
      else
        tmp &= var;
    }

    if ( sig )
    {
      output = tmp;
      sig = 0;
    }
    else
      output |= tmp;
  }
  return output;
}

BDD create_bdd_from_tt_str( Cudd& cudd, std::string tt_str, uint32_t num_inputs )
{
  //std::ifstream infile( file_name );
  //std::string tt_str;
  //infile >> tt_str;
  
  auto bddNodes = new BDD[num_inputs];
  for ( int i = num_inputs - 1; i >= 0; i-- )
  {
    bddNodes[i] = cudd.bddVar(); // index 0: LSB
  }

  BDD f_bdd;
  int sig = 1;

  /* 
    zero index in tt_str consist the bigest minterm of tt 
    so we read it from last element
  */
  
  for ( auto i = 0u; i <tt_str.size() ; i++ )
  {
    if ( tt_str[tt_str.size()-i-1] == '1' )
    {
      auto n = i;
      BDD temp;
      temp = ( ( n & 1 ) == 1 ) ? bddNodes[0] : !bddNodes[0];
      n >>= 1;
      for ( auto j = 1u; j < num_inputs; j++ )
      {
        temp &= ( ( n & 1 ) == 1 ) ? bddNodes[j] : !bddNodes[j];
        n >>= 1;
      }

      if ( sig )
      {
        f_bdd = temp;
        sig = 0;
      }
      else
      {
        f_bdd |= temp;
      }
    }
  }

  return f_bdd;
}

BDD create_bdd( Cudd& cudd, std::string str, create_bdd_param bdd_param, uint32_t& num_inputs )
{
  BDD bdd;
  if ( bdd_param.strategy == create_bdd_param::strategy::create_from_tt )
    bdd = create_bdd_from_tt_str( cudd, str, num_inputs );
  else
    bdd = create_bdd_from_pla( cudd, str, num_inputs );

  return bdd;
}

void draw_dump( DdNode* f_add, DdManager* mgr )
{
  FILE* outfile; /* output file pointer for .dot file */
  outfile = fopen( "graph.dot", "w" );
  DdNode** ddnodearray = (DdNode**)malloc( sizeof( DdNode* ) ); /* initialize the function array */
  ddnodearray[0] = f_add;
  Cudd_DumpDot( mgr, 1, ddnodearray, NULL, NULL, outfile ); /* dump the function to .dot file */
}

void count_ones_add_nodes( std::unordered_set<DdNode*>& visited,
                           std::vector<std::map<DdNode*, uint32_t>>& node_ones,
                           DdNode* f, uint32_t num_vars, std::vector<uint32_t> orders )
{
  auto current = f;
  if ( visited.count( current ) )
    return;
  if ( Cudd_IsConstant( current ) )
    return;

  count_ones_add_nodes( visited, node_ones, cuddE( current ), num_vars, orders );
  count_ones_add_nodes( visited, node_ones, cuddT( current ), num_vars, orders );

  visited.insert( current );
  uint32_t Eones = 0;
  uint32_t Tones = 0;

  if ( Cudd_IsConstant( cuddT( current ) ) )
  {
    if ( Cudd_V( cuddT( current ) ) )
    {
      auto temp_pow = num_vars - orders[current->index] - 1;
      Tones = pow( 2, temp_pow ) * 1;
    }
  }
  else
  {
    auto temp_pow = orders[cuddT( current )->index] - 1 - orders[current->index];
    auto Tvalue = 0;
    Tvalue = node_ones[cuddT( current )->index].find( cuddT( current ) )->second;
    Tones = pow( 2, temp_pow ) * Tvalue;
  }

  if ( Cudd_IsConstant( cuddE( current ) ) )
  {
    if ( Cudd_V( cuddE( current ) ) )
    {
      auto temp_pow = num_vars - orders[current->index] - 1;
      Eones = pow( 2, temp_pow ) * 1;
    }
  }
  else
  {
    auto temp_pow = orders[cuddE( current )->index] - 1 - orders[current->index];
    //auto max_ones = pow( 2, num_vars - orders[cuddE( current )->index] );
    auto Evalue = 0;
    Evalue = node_ones[cuddE( current )->index].find( cuddE( current ) )->second;
    Eones = pow( 2, temp_pow ) * Evalue;
  }

  node_ones[current->index].insert( {current, Tones + Eones} );
}

void count_ones_add_nodes( std::unordered_set<DdNode*>& visited,
                           std::vector<std::map<DdNode*, uint32_t>>& node_ones,
                           DdNode* f, uint32_t num_vars )
{
  auto current = f;
  if ( visited.count( current ) )
    return;
  if ( Cudd_IsConstant( current ) )
    return;

  count_ones_add_nodes( visited, node_ones, cuddE( current ), num_vars );
  count_ones_add_nodes( visited, node_ones, cuddT( current ), num_vars );

  visited.insert( current );
  uint32_t Eones = 0;
  uint32_t Tones = 0;

  if ( Cudd_IsConstant( cuddT( current ) ) )
  {
    if ( Cudd_V( cuddT( current ) ) )
    {
      auto temp_pow = num_vars - current->index - 1;
      Tones = pow( 2, temp_pow ) * 1;
    }
  }
  else
  {
    auto temp_pow = cuddT( current )->index - 1 - current->index;
    auto Tvalue = 0;
    Tvalue = node_ones[cuddT( current )->index].find( cuddT( current ) )->second;
    Tones = pow( 2, temp_pow ) * Tvalue;
  }

  if ( Cudd_IsConstant( cuddE( current ) ) )
  {
    if ( Cudd_V( cuddE( current ) ) )
    {
      auto temp_pow = num_vars - current->index - 1;
      Eones = pow( 2, temp_pow ) * 1;
    }
  }
  else
  {
    auto temp_pow = cuddE( current )->index - 1 - current->index;
    //auto max_ones = pow( 2, num_vars - orders[cuddE( current )->index] );
    auto Evalue = 0;
    Evalue = node_ones[cuddE( current )->index].find( cuddE( current ) )->second;
    Eones = pow( 2, temp_pow ) * Evalue;
  }

  node_ones[current->index].insert( {current, Tones + Eones} );
}

void extract_probabilities_and_MCgates( std::unordered_set<DdNode*>& visited,
                                        std::vector<std::map<DdNode*, uint32_t>> node_ones,
                                        std::map<DdNode*, std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>>& gates,
                                        DdNode* f, uint32_t num_vars, std::vector<uint32_t> orders )
{
  /* gates construction
  std::map<DdNode*, std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>>
  map -> for each node
  std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>
  vector1 -> include all qubits
  vector2 -> gates for each qubit
  inner vector -> controls
  */
  auto current = f;
  if ( visited.count( current ) )
    return;
  if ( Cudd_IsConstant( current ) )
    return;

  extract_probabilities_and_MCgates( visited, node_ones, gates, cuddE( current ), num_vars, orders );
  extract_probabilities_and_MCgates( visited, node_ones, gates, cuddT( current ), num_vars, orders );

  visited.insert( current );
  double ep = 0.0;
  double dp = node_ones[current->index].find( current )->second;
  if ( Cudd_IsConstant( cuddT( current ) ) && Cudd_V( cuddT( current ) ) ) //?? ->  probability
  {
    ep = pow( 2, num_vars - 1 - current->index ); //ep = pow( 2, num_vars - 1 - orders[current->index] );
  }
  else if ( Cudd_IsConstant( cuddT( current ) ) && !Cudd_V( cuddT( current ) ) ) //?? ->  probability
  {
    ep = 0;
  }
  else
  {
    ep = node_ones[cuddT( current )->index].find( cuddT( current ) )->second *
         ( pow( 2, cuddT( current )->index - current->index - 1 ) ); //?? ->  probability // ( pow( 2, orders[cuddT( current )->index] - orders[current->index] - 1 ) ); 
  }

  double p = ep / dp; /* one probability */

  auto const it = gates.find( current );
  if ( it == gates.end() )
  {
    gates.emplace( current, std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>( num_vars ) );
  }

  /* inserting current single-qubit G(p) gate */
  if ( p != 0 )
  {
    gates[current][current->index].emplace_back( std::make_pair<double, std::vector<int32_t>>( 1 - p, {} ) );
    //std::cout << "index: " << current->index << std::endl;
  }

  /* inserting childs gates */
  if ( !Cudd_IsConstant( cuddE( current ) ) )
  {
    for ( auto i = 0u; i < num_vars; i++ )
    {
      if ( gates[cuddE( current )][i].size() != 0 )
      {
        for ( auto j = 0u; j < gates[cuddE( current )][i].size(); j++ )
        {
          auto pro = gates[cuddE( current )][i][j].first;
          std::vector<int32_t> controls;
          for ( auto k = 0u; k < gates[cuddE( current )][i][j].second.size(); k++ )
            controls.emplace_back( gates[cuddE( current )][i][j].second[k] );
          //std::copy(gates[cuddE(current)][i][j].second.begin() , gates[cuddE(current)][i][j].second.end() , controls);
          //auto controls = gates[cuddE(current)][i][j].second;
          if ( p != 0 && p != 1 )
          {
            //std::cout << "p: " << p << std::endl;
            controls.emplace_back( -( current->index + 1 ) );
          }

          gates[current][i].emplace_back( std::make_pair( pro, controls ) );
        }
      }
    }
  }

  if ( !Cudd_IsConstant( cuddT( current ) ) )
  {
    for ( auto i = 0u; i < num_vars; i++ )
    {
      if ( gates[cuddT( current )][i].size() != 0 )
      {
        for ( auto j = 0u; j < gates[cuddT( current )][i].size(); j++ )
        {
          auto pro = gates[cuddT( current )][i][j].first;
          std::vector<int32_t> controls;
          for ( auto k = 0u; k < gates[cuddT( current )][i][j].second.size(); k++ )
            controls.emplace_back( gates[cuddT( current )][i][j].second[k] );
          //std::copy(gates[cuddT(current)][i][j].second.begin() , gates[cuddT(current)][i][j].second.end() , controls);

          //auto controls = gates[cuddT(current)][i][j].second;
          if ( p != 0 && p != 1 )
            controls.emplace_back( current->index + 1 );
          gates[current][i].emplace_back( std::make_pair( pro, controls ) );
        }
      }
    }
  }

  /* inserting Hadamard gates */
  auto Edown = Cudd_IsConstant( cuddE( current ) ) ? num_vars : cuddE( current )->index; //orders[cuddE( current )->index];
  auto Tdown = Cudd_IsConstant( cuddT( current ) ) ? num_vars : cuddT( current )->index; //orders[cuddT( current )->index];
  uint32_t E_num_value = 1;
  uint32_t T_num_value = 1;
  if ( Cudd_IsConstant( cuddE( current ) ) && !Cudd_V( cuddE( current ) ) )
    E_num_value = 0;
  if ( Cudd_IsConstant( cuddT( current ) ) && !Cudd_V( cuddT( current ) ) )
    T_num_value = 0;
  for ( auto i = current->index + 1; i < Edown; i++ ) //orders[current->index] + 1
  {
    //auto id = std::find( orders.begin(), orders.end(), i ) - orders.begin();
    if ( E_num_value != 0 )
    {
      //gates_pre[id].insert({current, std::make_tuple( 1/2.0, nullptr , nullptr ) });
      std::vector<int32_t> temp_c;
      if ( p != 0 && p != 1 )
        temp_c.emplace_back( -( current->index + 1 ) );
      gates[current][i].emplace_back( std::make_pair( 1 / 2.0, temp_c ) ); // id -> i
    }
  }
  for ( auto i = current->index + 1; i < Tdown; i++ ) //orders[current->index] + 1;
  {
    //auto id = std::find( orders.begin(), orders.end(), i ) - orders.begin();
    if ( T_num_value != 0 )
    {
      //gates_pre[id].insert({current, std::make_tuple( 1/2.0, nullptr , nullptr ) });
      std::vector<int32_t> temp_c;
      if ( p != 0 && p != 1 )
        temp_c.emplace_back( current->index + 1 );
      gates[current][i].emplace_back( std::make_pair( 1 / 2.0, temp_c ) ); //id -> i
    }
  }
}

void extract_probabilities_and_MCgates( std::unordered_set<DdNode*>& visited,
                                        std::vector<std::map<DdNode*, uint32_t>> node_ones,
                                        std::map<DdNode*, std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>>& gates,
                                        DdNode* f, uint32_t num_vars)
{
  /* gates construction
  std::map<DdNode*, std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>>
  map -> for each node
  std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>
  vector1 -> include all qubits
  vector2 -> gates for each qubit
  inner vector -> controls
  */
  auto current = f;
  if ( visited.count( current ) )
    return;
  if ( Cudd_IsConstant( current ) )
    return;

  extract_probabilities_and_MCgates( visited, node_ones, gates, cuddE( current ), num_vars );
  extract_probabilities_and_MCgates( visited, node_ones, gates, cuddT( current ), num_vars );

  visited.insert( current );
  double ep = 0.0;
  double dp = node_ones[current->index].find( current )->second;
  if ( Cudd_IsConstant( cuddT( current ) ) && Cudd_V( cuddT( current ) ) ) //?? ->  probability
  {
    ep = pow( 2, num_vars - 1 - current->index ); //ep = pow( 2, num_vars - 1 - orders[current->index] );
  }
  else if ( Cudd_IsConstant( cuddT( current ) ) && !Cudd_V( cuddT( current ) ) ) //?? ->  probability
  {
    ep = 0;
  }
  else
  {
    ep = node_ones[cuddT( current )->index].find( cuddT( current ) )->second *
         ( pow( 2, cuddT( current )->index - current->index - 1 ) ); //?? ->  probability // ( pow( 2, orders[cuddT( current )->index] - orders[current->index] - 1 ) ); 
  }

  double p = ep / dp; /* one probability */

  auto const it = gates.find( current );
  if ( it == gates.end() )
  {
    gates.emplace( current, std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>( num_vars ) );
  }

  /* inserting current single-qubit G(p) gate */
  if ( p != 0 )
  {
    gates[current][current->index].emplace_back( std::make_pair<double, std::vector<int32_t>>( 1 - p, {} ) );
    //std::cout << "index: " << current->index << std::endl;
  }

  /* inserting childs gates */
  if ( !Cudd_IsConstant( cuddE( current ) ) )
  {
    for ( auto i = 0u; i < num_vars; i++ )
    {
      if ( gates[cuddE( current )][i].size() != 0 )
      {
        for ( auto j = 0u; j < gates[cuddE( current )][i].size(); j++ )
        {
          auto pro = gates[cuddE( current )][i][j].first;
          std::vector<int32_t> controls;
          for ( auto k = 0u; k < gates[cuddE( current )][i][j].second.size(); k++ )
            controls.emplace_back( gates[cuddE( current )][i][j].second[k] );
          //std::copy(gates[cuddE(current)][i][j].second.begin() , gates[cuddE(current)][i][j].second.end() , controls);
          //auto controls = gates[cuddE(current)][i][j].second;
          if ( p != 0 && p != 1 )
          {
            //std::cout << "p: " << p << std::endl;
            controls.emplace_back( -( current->index + 1 ) );
          }

          gates[current][i].emplace_back( std::make_pair( pro, controls ) );
        }
      }
    }
  }

  if ( !Cudd_IsConstant( cuddT( current ) ) )
  {
    for ( auto i = 0u; i < num_vars; i++ )
    {
      if ( gates[cuddT( current )][i].size() != 0 )
      {
        for ( auto j = 0u; j < gates[cuddT( current )][i].size(); j++ )
        {
          auto pro = gates[cuddT( current )][i][j].first;
          std::vector<int32_t> controls;
          for ( auto k = 0u; k < gates[cuddT( current )][i][j].second.size(); k++ )
            controls.emplace_back( gates[cuddT( current )][i][j].second[k] );
          //std::copy(gates[cuddT(current)][i][j].second.begin() , gates[cuddT(current)][i][j].second.end() , controls);

          //auto controls = gates[cuddT(current)][i][j].second;
          if ( p != 0 && p != 1 )
            controls.emplace_back( current->index + 1 );
          gates[current][i].emplace_back( std::make_pair( pro, controls ) );
        }
      }
    }
  }

  /* inserting Hadamard gates */
  auto Edown = Cudd_IsConstant( cuddE( current ) ) ? num_vars : cuddE( current )->index; //orders[cuddE( current )->index];
  auto Tdown = Cudd_IsConstant( cuddT( current ) ) ? num_vars : cuddT( current )->index; //orders[cuddT( current )->index];
  uint32_t E_num_value = 1;
  uint32_t T_num_value = 1;
  if ( Cudd_IsConstant( cuddE( current ) ) && !Cudd_V( cuddE( current ) ) )
    E_num_value = 0;
  if ( Cudd_IsConstant( cuddT( current ) ) && !Cudd_V( cuddT( current ) ) )
    T_num_value = 0;
  for ( auto i = current->index + 1; i < Edown; i++ ) //orders[current->index] + 1
  {
    //auto id = std::find( orders.begin(), orders.end(), i ) - orders.begin();
    if ( E_num_value != 0 )
    {
      //gates_pre[id].insert({current, std::make_tuple( 1/2.0, nullptr , nullptr ) });
      std::vector<int32_t> temp_c;
      if ( p != 0 && p != 1 )
        temp_c.emplace_back( -( current->index + 1 ) );
      gates[current][i].emplace_back( std::make_pair( 1 / 2.0, temp_c ) ); // id -> i
    }
  }
  for ( auto i = current->index + 1; i < Tdown; i++ ) //orders[current->index] + 1;
  {
    //auto id = std::find( orders.begin(), orders.end(), i ) - orders.begin();
    if ( T_num_value != 0 )
    {
      //gates_pre[id].insert({current, std::make_tuple( 1/2.0, nullptr , nullptr ) });
      std::vector<int32_t> temp_c;
      if ( p != 0 && p != 1 )
        temp_c.emplace_back( current->index + 1 );
      gates[current][i].emplace_back( std::make_pair( 1 / 2.0, temp_c ) ); //id -> i
    }
  }
}

void extract_quantum_gates( DdNode* f_add, uint32_t num_inputs, std::vector<uint32_t> orders,
                            std::map<DdNode*, std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>>& gates )
{
  std::vector<std::map<DdNode*, uint32_t>> node_ones( num_inputs );
  //std::vector<std::pair<uint32_t, uint32_t>> ones_frac;
  std::unordered_set<DdNode*> visited;
  std::unordered_set<DdNode*> visited1;
  count_ones_add_nodes( visited, node_ones, f_add, num_inputs );
  extract_probabilities_and_MCgates( visited1, node_ones, gates, f_add, num_inputs );
}

void extract_statistics( Cudd cudd, DdNode* f_add,
                         std::map<DdNode*, std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>> gates,
                         qsp_add_statistics& stats, std::vector<uint32_t> orders )
{
  auto total_MC_gates = 0u;
  auto Rxs = 0;
  auto Rys = 0;
  auto Ts = 0;
  auto CNOTs = 0;
  auto Ancillaes = 0;

  for ( auto i = 0; i < cudd.ReadSize(); i++ )
  {
    
    if(gates[f_add].size()==0)
      break;
    total_MC_gates += gates[f_add][orders[i]].size();
    
    
    if ( gates[f_add][orders[i]].size() == 0 )
      continue;
    
    auto max_cs = 0u;
    std::vector< std::vector<int32_t> > MCs;
    for ( auto j = 0u; j < gates[f_add][orders[i]].size(); j++ )
    {
      if(gates[f_add][orders[i]][j].second.size()>0)
        MCs.emplace_back(gates[f_add][orders[i]][j].second);
      //auto cs = gates[f_add][i][j].second.size();
      //if ( cs > max_cs )
        //max_cs = cs;
    }
    if(MCs.size()>0)
      max_cs = extract_max_controls(MCs);
    
    if ( max_cs == 0 )
    {
      Rys += 1;
    }
    else if ( max_cs == 1 && gates[f_add][orders[i]].size() == 1 && gates[f_add][orders[i]][0].first == 0 )
    {
      CNOTs += 1;
    }

    else
    {
      CNOTs += pow( 2, max_cs );
      Rys += pow( 2, max_cs );
    }
  }

  stats.MC_gates += (total_MC_gates==0 ? 0 : total_MC_gates-1);
  stats.cnots += CNOTs;
  stats.sqgs += ( Rxs + Rys + Ts );
  stats.ancillaes += Ancillaes;
}

} // namespace detail
//**************************************************************

/**
 * \breif Quantum State Preparation using Decision Diagram
 * 
 * \tparam Network the type of generated quantum circuit
 * \param network the extracted quantum circuit for given quantum state
 * \param str include desired quantum state for preparation in tt or pla version
 * \param stats store all desired statistics of quantum state preparation process
 * \param param specify some parameters for qsp such as creating BDD from tt or pla
*/
template<class Network>
void qsp_add( Network& network, std::string str, qsp_add_statistics& stats, create_bdd_param param = {} )
{
  uint32_t num_inputs = log2(str.size());
  /* reordering tt */
  std::vector<uint32_t> orders;
  for ( int32_t i = num_inputs-1; i >=0; i-- )
    orders.emplace_back( i );

    // orders.emplace_back( 0 );
    // orders.emplace_back( 2 );
    // orders.emplace_back( 1 );
    // orders.emplace_back( 3 );
  //std::cout<<orders[0]<<" "<<orders[1]<<" "<<orders[2]<<" "<<orders[3]<<std::endl;
  kitty::dynamic_truth_table tt(num_inputs);
  kitty::create_from_binary_string(tt, str);
  reordering_on_tt_inplace(tt, orders);
  str = kitty::to_binary(tt);
  //std::cout<<str<<std::endl;
  /* Create BDD */
  Cudd cudd;
  auto mgr = cudd.getManager();
  auto f_bdd = detail::create_bdd( cudd, str, param, num_inputs );
  
  //auto f_bdd = detail::create_bdd_from_tt_str( cudd, str, num_inputs );
  auto f_add = Cudd_BddToAdd( mgr, f_bdd.getNode() );

  /* 
    BDD help sample 
    auto d = mgr.bddVar(); //MSB
    auto c = mgr.bddVar(); //LSB
  */

  /* draw add in a output file */
  detail::draw_dump( f_add, mgr );
  
  /* Generate quantum gates by traversing ADD */
  
  std::map<DdNode*, std::vector<std::vector<std::pair<double, std::vector<int32_t>>>>> gates;

  stopwatch<>::duration_type time_add_traversal{0};
  {
    stopwatch t( time_add_traversal );
    detail::extract_quantum_gates( f_add, num_inputs, orders, gates );
  }

  /* extract statistics */
  stats.nodes += Cudd_DagSize( f_add ) - 2; // it consider 2 nodes for "0" and "1"
  stats.time += to_seconds( time_add_traversal );
  detail::extract_statistics( cudd, f_add, gates, stats, orders );
  
}

} // namespace angel
