#pragma once

#include <kitty/dynamic_truth_table.hpp>
#include <kitty/npn.hpp>
#include <angel/utils/helper_functions.hpp>
#include <angel/utils/stopwatch.hpp>
#include <fmt/format.h>
#include <iostream>
#include <map>
#include <vector>
#include <unordered_map>
#include <angel/dependency_analysis/common.hpp>

namespace angel
{
using pattern_based_dependencies_t = std::map< uint32_t, dependency_analysis_types::pattern >;
using esop_based_dependencies_t = std::map< uint32_t, std::vector<std::vector<uint32_t>> >;
using gates_t = std::map<uint32_t, std::vector<std::pair<double, std::vector<uint32_t>>>>;
using order_t = std::vector<uint32_t>;

struct qsp_1bench_stats
{
    stopwatch<>::duration_type total_time{0};
    uint32_t total_cnots{0};
    uint32_t total_rys{0};
    uint32_t total_nots{0};
    std::pair<uint32_t, uint32_t> gates_count = std::make_pair(0,0);
};



struct qsp_general_stats
{
    /* cache declaration */
    std::unordered_map<uint64_t, qsp_1bench_stats> cache;

    stopwatch<>::duration_type total_time{0};
    uint32_t total_bench{0};
    uint32_t has_no_dependencies{0};
    uint32_t no_dependencies_computed{0};
    uint32_t has_dependencies{0};
    uint32_t funcdep_bench_useful{0};
    uint32_t funcdep_bench_notuseful{0};
    uint32_t total_cnots{0};
    uint32_t total_rys{0};
    uint32_t total_nots{0};
    std::vector<std::pair<uint32_t, uint32_t>> gates_count = {};

    void report(std::ostream &os = std::cout) const
    {
        os << "[i] number of analyzed benchmarks = " << total_bench << std::endl;
        os << fmt::format("[i] total = no deps exist + no deps found + found deps ::: {} = {} + {} + {}\n",
                          (has_no_dependencies + no_dependencies_computed +
                           has_dependencies),
                          has_no_dependencies, no_dependencies_computed,
                          has_dependencies);
        os << fmt::format("[i] total deps = dep useful + dep not useful ::: {} = {} + {}\n",
                          funcdep_bench_useful + funcdep_bench_notuseful, funcdep_bench_useful, funcdep_bench_notuseful);
        os << fmt::format("[i] total synthesis time (considering dependencies) = {:8.2f}s\n",
                          to_seconds( total_time ));

        os << fmt::format("[i] synthesis result: CNOTs / RYs / NOTs = {} / {} / {} \n",
                          total_cnots, total_rys, total_nots);
        os << fmt::format("[i] synthesis result: CNOTs / SQgates = {} / {} \n",
                          total_cnots, total_rys + total_nots);
        /// Compute Avg and SD for CNOTs and SQgates
        auto sum_f = 0; /// CNOTs: first
        auto sum_s = 0; /// SQgates: second
        for (auto n = 0u; n < gates_count.size(); n++)
        {
            sum_f += gates_count[n].first;
            sum_s += gates_count[n].second;
        }

        double avg_f = sum_f / double(gates_count.size());
        double avg_s = sum_s / double(gates_count.size());

        auto var_f = 0.0;
        auto var_s = 0.0;
        for (auto n = 0u; n < gates_count.size(); n++)
        {
            var_f += (double(gates_count[n].first) - avg_f) * (double(gates_count[n].first) - avg_f);
            var_s += (double(gates_count[n].second) - avg_s) * (double(gates_count[n].second) - avg_s);
        }
        var_f /= gates_count.size();
        var_s /= gates_count.size();
        auto sd_f = sqrt(var_f);
        auto sd_s = sqrt(var_s);
        os << fmt::format("[i] Avg Cnots = {}\n", avg_f);
        os << fmt::format("[i] Var Cnots = {}\n", var_f);
        os << fmt::format("[i] SD Cnots = {}\n", sd_f);

        os << fmt::format("[i] Avg SQgates = {}\n", avg_s);
        os << fmt::format("[i] Var SQgates = {}\n", var_s);
        os << fmt::format("[i] SD SQgates = {}\n", sd_s);
    }
};

// struct deps_operation_stats
// {
//   /* Be verbose */
//   bool verbose = true;

//   /* Map pattern name to number of occurrencies */
//   mutable std::unordered_map<std::string, uint32_t> pattern_occurrence;

//   void report( std::ostream &os = std::cout ) const
//   {
//     for ( const auto& d : pattern_occurrence )
//     {
//       if ( d.second > 0u || verbose )
//       {
//         os << fmt::format( "[i] number of {:6s} operation: {:7d}\n", d.first, d.second );
//       }
//     }
//   }
// };

// void extract_deps_operation_stats( deps_operation_stats& op_stats, dependencies_t const& deps )
// {
//   for( auto i = 0u; i < deps.size(); ++i )
//   {
//     if ( deps.find( i ) == deps.end() )
//       continue;

//     op_stats.pattern_occurrence[fmt::format( "{}-{}", deps.at( i ).first, deps.at( i ).second.size() )]++;
//   }
// }

namespace detail
{

std::pair<uint32_t, uint32_t> esop_cost( std::vector<std::vector<uint32_t>> const& esop )
  {
    assert( esop.size() > 0u );
    std::pair<uint32_t, uint32_t> cnots_sqgs = {0, 0};
    /// first AND pattern
    auto const n0 = esop[0].size();
    auto polarity_counter = 0u;
    for ( auto i = 0u; i < n0; ++i )
    {
      polarity_counter += 2u * ( esop[0][i] % 2 );
    }
    cnots_sqgs.first += ( 1 << n0 );
    cnots_sqgs.second += ( 1 << n0 );
    cnots_sqgs.second += polarity_counter;

    /// the rest
    for ( auto i = 1u; i < esop.size(); i++ )
    {
      auto const n = esop[i].size();
      auto polarity_counter = 0u;
      for ( auto j = 0u; j < n; ++j )
      {
        polarity_counter += 2u * ( esop[i][j] % 2 );
      }
      cnots_sqgs.first += ( 1 << ( n + 1 ) );
      cnots_sqgs.second += ( 1 << ( n + 1 ) );
      cnots_sqgs.second += polarity_counter;
    }

    return cnots_sqgs;
  }

}
/* with esop based dependencies */
void MC_qg_generation(gates_t &gates, uint32_t num_vars, kitty::dynamic_truth_table tt, uint32_t var_index, std::vector<uint32_t> controls,
                      esop_based_dependencies_t dependencies, std::vector<uint32_t> zero_lines, std::vector<uint32_t> one_lines)
{
    /*-----co factors-------*/
    kitty::dynamic_truth_table tt0(var_index);
    kitty::dynamic_truth_table tt1(var_index);

    tt0 = kitty::shrink_to(kitty::cofactor0(tt, var_index), var_index);
    tt1 = kitty::shrink_to(kitty::cofactor1(tt, var_index), var_index);

    /*--computing probability gate---*/
    auto c0_ones = kitty::count_ones(tt0);
    auto c1_ones = kitty::count_ones(tt1);
    auto tt_ones = kitty::count_ones(tt);
    bool is_const = 0;
    auto it0 = std::find(zero_lines.begin(), zero_lines.end(), var_index);
    auto it1 = std::find(one_lines.begin(), one_lines.end(), var_index);
    if (it1 != one_lines.end() ) // insert not gate
    {
        if( gates.find(var_index)==gates.end() )
        {
            gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
        }
        is_const = 1;
    }
    else if (it0 != zero_lines.end()) // inzert zero gate
    {
        is_const = 1;
    }
    else if (c0_ones != tt_ones)
    { /* == --> identity and ignore */
        double angle = 2 * acos(sqrt(static_cast<double>(c0_ones) / tt_ones));
        //angle *= (180/3.14159265); //in degree
        /*----add probability gate----*/
        auto it = dependencies.find(var_index);
        bool deps_useful = false;
        if (it != dependencies.end() )
        {
           auto const [esop_cnots, esop_sqgs] = detail::esop_cost(dependencies[var_index]); 
           if(esop_cnots <= (pow(2, num_vars-var_index-1)))
           {
               deps_useful = true;
           }
        }

        if (gates[var_index].size() == 0 && deps_useful)
        {
            for(auto const& inner: dependencies[var_index])
            {
                gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{inner}}); 
            }  
        }
        
        else
            gates[var_index].emplace_back(std::pair{angle, controls});
    }

    /*-----qc of cofactors-------*/
    /*---check state---*/
    auto c0_allone = (c0_ones == pow(2, tt0.num_vars())) ? true : false;
    auto c0_allzero = (c0_ones == 0) ? true : false;
    auto c1_allone = (c1_ones == pow(2, tt1.num_vars())) ? true : false;
    auto c1_allzero = (c1_ones == 0) ? true : false;

    std::vector<uint32_t> controls_new0;
    std::copy(controls.begin(), controls.end(), back_inserter(controls_new0));
    if (dependencies.find(var_index) == dependencies.end() && !is_const)
    {
        auto ctrl0 = var_index * 2 + 1; /* negetive control: /2 ---> index %2 ---> sign */
        controls_new0.emplace_back(ctrl0);
    }
    std::vector<uint32_t> controls_new1;
    std::copy(controls.begin(), controls.end(), back_inserter(controls_new1));
    if (dependencies.find(var_index) == dependencies.end() && !is_const)
    {
        auto ctrl1 = var_index * 2 + 0; /* positive control: /2 ---> index %2 ---> sign */
        controls_new1.emplace_back(ctrl1);
    }

    if (c0_allone)
    {
        /*---add H gates---*/
        for (auto i = 0u; i < var_index; i++)
            gates[i].emplace_back(std::pair{M_PI / 2, controls_new0});
        /*--check one cofactor----*/
        if (c1_allone)
        {
            /*---add H gates---*/
            for (auto i = 0u; i < var_index; i++)
                gates[i].emplace_back(std::pair{M_PI / 2, controls_new1});
        }
        else if (c1_allzero)
        {
            return;
        }
        else
        { /* some 1 some 0 */
            MC_qg_generation(gates, num_vars, tt1, var_index - 1, controls_new1, dependencies, zero_lines, one_lines);
        }
    }
    else if (c0_allzero)
    {
        /*--check one cofactor----*/
        if (c1_allone)
        {
            /*---add H gates---*/
            for (auto i = 0u; i < var_index; i++)
                gates[i].emplace_back(std::pair{M_PI / 2, controls_new1});
        }
        else if (c1_allzero)
        {
            return;
        }
        else
        { /* some 1 some 0 */
            MC_qg_generation(gates, num_vars, tt1, var_index - 1, controls_new1, dependencies, zero_lines, one_lines);
        }
    }
    else
    { /* some 0 some 1 for c0 */
        if (c1_allone)
        {
            MC_qg_generation(gates, num_vars, tt0, var_index - 1, controls_new0, dependencies, zero_lines, one_lines);
            /*---add H gates---*/
            for (auto i = 0u; i < var_index; i++)
                gates[i].emplace_back(std::pair{M_PI / 2, controls_new1});
        }
        else if (c1_allzero)
        {
            MC_qg_generation(gates, num_vars, tt0, var_index - 1, controls_new0, dependencies, zero_lines, one_lines);
        }
        else
        { /* some 1 some 0 */
            MC_qg_generation(gates, num_vars, tt0, var_index - 1, controls_new0, dependencies, zero_lines, one_lines);
            MC_qg_generation(gates, num_vars, tt1, var_index - 1, controls_new1, dependencies, zero_lines, one_lines);
        }
    }
}

/* with pattern based dependencies */
void MC_qg_generation(gates_t &gates, uint32_t num_vars, kitty::dynamic_truth_table tt, uint32_t var_index, std::vector<uint32_t> controls,
                      pattern_based_dependencies_t dependencies, std::vector<uint32_t> zero_lines, std::vector<uint32_t> one_lines)
{
    /*-----co factors-------*/
    kitty::dynamic_truth_table tt0(var_index);
    kitty::dynamic_truth_table tt1(var_index);

    tt0 = kitty::shrink_to(kitty::cofactor0(tt, var_index), var_index);
    tt1 = kitty::shrink_to(kitty::cofactor1(tt, var_index), var_index);

    /*--computing probability gate---*/
    auto c0_ones = kitty::count_ones(tt0);
    auto c1_ones = kitty::count_ones(tt1);
    auto tt_ones = kitty::count_ones(tt);
    bool is_const = 0;
    auto it0 = std::find(zero_lines.begin(), zero_lines.end(), var_index);
    auto it1 = std::find(one_lines.begin(), one_lines.end(), var_index);
    if (it1 != one_lines.end() ) // insert not gate
    {
        if( gates.find(var_index)==gates.end() )
        {
            gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
        }
        is_const = 1;
    }
    else if (it0 != zero_lines.end()) // inzert zero gate
    {
        is_const = 1;
    }
    else if (c0_ones != tt_ones)
    { /* == --> identity and ignore */
        double angle = 2 * acos(sqrt(static_cast<double>(c0_ones) / tt_ones));
        //angle *= (180/3.14159265); //in degree
        /*----add probability gate----*/
        auto it = dependencies.find(var_index);

        if (it != dependencies.end())
        {
            if (gates[var_index].size() == 0)
            {

                if (dependencies[var_index].first == dependency_analysis_types::pattern_kind::EQUAL)
                {
                    if(dependencies[var_index].second[0] % 2 == 0) /* equal operation */
                    {
                        gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{dependencies[var_index].second}});
                    }
                    else /* not operation */
                    {
                        gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{dependencies[var_index].second}});
                        gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
                    }
                        
                }

                else if (dependencies[var_index].first == dependency_analysis_types::pattern_kind::XOR)
                {
                    for (auto d_in = 0u; d_in < dependencies[var_index].second.size(); d_in++)
                    {
                        if(dependencies[var_index].second[d_in] % 2 == 1) /* it is temporary */
                        {
                            gates[dependencies[var_index].second[d_in]].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
                            gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{dependencies[var_index].second[d_in]-1}}); /// modifying control line
                            gates[dependencies[var_index].second[d_in]].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
                        }
                        else
                        {
                            gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{dependencies[var_index].second[d_in]}});
                        }   
                    }     
                }

                else if (dependencies[var_index].first == dependency_analysis_types::pattern_kind::XNOR)
                {
                    for (auto d_in = 0u; d_in < dependencies[var_index].second.size(); d_in++)
                    {
                        if(dependencies[var_index].second[d_in] % 2 == 1) /* it is temporary */
                        {
                            gates[dependencies[var_index].second[d_in]].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
                            gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{dependencies[var_index].second[d_in]-1}});/// modifying control line
                            gates[dependencies[var_index].second[d_in]].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
                        }
                        else
                        {
                            gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{dependencies[var_index].second[d_in]}});
                        }   
                    }     
                    gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
                }

                else if (dependencies[var_index].first == dependency_analysis_types::pattern_kind::AND)
                {
                    std::vector<uint32_t> positive_ctrls;
                    for (auto d_in = 0u; d_in < dependencies[var_index].second.size(); d_in++) /// insert nots
                    {
                        if(dependencies[var_index].second[d_in] % 2 == 1) /* it is temporary */
                        {
                            gates[dependencies[var_index].second[d_in]].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
                            positive_ctrls.emplace_back(dependencies[var_index].second[d_in]-1);
                        }
                        else
                        {
                            positive_ctrls.emplace_back(dependencies[var_index].second[d_in]);
                        }   
                    }
                    
                    gates[var_index].emplace_back(std::pair{M_PI, positive_ctrls}); /// insert and

                    for (auto d_in = 0u; d_in < dependencies[var_index].second.size(); d_in++) /// insert nots
                    {
                        if(dependencies[var_index].second[d_in] % 2 == 1) /* it is temporary */
                        {
                            gates[dependencies[var_index].second[d_in]].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
                        }  
                    }
                }

                else if (dependencies[var_index].first == dependency_analysis_types::pattern_kind::NAND)
                {
                    std::vector<uint32_t> positive_ctrls;
                    for (auto d_in = 0u; d_in < dependencies[var_index].second.size(); d_in++) /// insert nots
                    {
                        if(dependencies[var_index].second[d_in] % 2 == 1) /* it is temporary */
                        {
                            gates[dependencies[var_index].second[d_in]].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
                            positive_ctrls.emplace_back(dependencies[var_index].second[d_in]-1);
                        }
                        else
                        {
                            positive_ctrls.emplace_back(dependencies[var_index].second[d_in]);
                        }   
                    }
                    
                    gates[var_index].emplace_back(std::pair{M_PI, positive_ctrls}); /// insert and

                    for (auto d_in = 0u; d_in < dependencies[var_index].second.size(); d_in++) /// insert nots
                    {
                        if(dependencies[var_index].second[d_in] % 2 == 1) /* it is temporary */
                        {
                            gates[dependencies[var_index].second[d_in]].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
                        }  
                    }

                    gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}}); /// insert not for and
                }
            
            }
        }

        else
            gates[var_index].emplace_back(std::pair{angle, controls});
    }

    /*-----qc of cofactors-------*/
    /*---check state---*/
    auto c0_allone = (c0_ones == pow(2, tt0.num_vars())) ? true : false;
    auto c0_allzero = (c0_ones == 0) ? true : false;
    auto c1_allone = (c1_ones == pow(2, tt1.num_vars())) ? true : false;
    auto c1_allzero = (c1_ones == 0) ? true : false;

    std::vector<uint32_t> controls_new0;
    std::copy(controls.begin(), controls.end(), back_inserter(controls_new0));
    if (dependencies.find(var_index) == dependencies.end() && !is_const)
    {
        auto ctrl0 = var_index * 2 + 1; /* negetive control: /2 ---> index %2 ---> sign */
        controls_new0.emplace_back(ctrl0);
    }
    std::vector<uint32_t> controls_new1;
    std::copy(controls.begin(), controls.end(), back_inserter(controls_new1));
    if (dependencies.find(var_index) == dependencies.end() && !is_const)
    {
        auto ctrl1 = var_index * 2 + 0; /* positive control: /2 ---> index %2 ---> sign */
        controls_new1.emplace_back(ctrl1);
    }

    if (c0_allone)
    {
        /*---add H gates---*/
        for (auto i = 0u; i < var_index; i++)
            gates[i].emplace_back(std::pair{M_PI / 2, controls_new0});
        /*--check one cofactor----*/
        if (c1_allone)
        {
            /*---add H gates---*/
            for (auto i = 0u; i < var_index; i++)
                gates[i].emplace_back(std::pair{M_PI / 2, controls_new1});
        }
        else if (c1_allzero)
        {
            return;
        }
        else
        { /* some 1 some 0 */
            MC_qg_generation(gates, num_vars, tt1, var_index - 1, controls_new1, dependencies, zero_lines, one_lines);
        }
    }
    else if (c0_allzero)
    {
        /*--check one cofactor----*/
        if (c1_allone)
        {
            /*---add H gates---*/
            for (auto i = 0u; i < var_index; i++)
                gates[i].emplace_back(std::pair{M_PI / 2, controls_new1});
        }
        else if (c1_allzero)
        {
            return;
        }
        else
        { /* some 1 some 0 */
            MC_qg_generation(gates, num_vars, tt1, var_index - 1, controls_new1, dependencies, zero_lines, one_lines);
        }
    }
    else
    { /* some 0 some 1 for c0 */
        if (c1_allone)
        {
            MC_qg_generation(gates, num_vars, tt0, var_index - 1, controls_new0, dependencies, zero_lines, one_lines);
            /*---add H gates---*/
            for (auto i = 0u; i < var_index; i++)
                gates[i].emplace_back(std::pair{M_PI / 2, controls_new1});
        }
        else if (c1_allzero)
        {
            MC_qg_generation(gates, num_vars, tt0, var_index - 1, controls_new0, dependencies, zero_lines, one_lines);
        }
        else
        { /* some 1 some 0 */
            MC_qg_generation(gates, num_vars, tt0, var_index - 1, controls_new0, dependencies, zero_lines, one_lines);
            MC_qg_generation(gates, num_vars, tt1, var_index - 1, controls_new1, dependencies, zero_lines, one_lines);
        }
    }
}

/* without dependencies */
void MC_qg_generation(gates_t &gates, kitty::dynamic_truth_table tt, uint32_t var_index, std::vector<uint32_t> controls,
                      std::vector<uint32_t> zero_lines, std::vector<uint32_t> one_lines)
{
    /*-----co factors-------*/
    kitty::dynamic_truth_table tt0(var_index);
    kitty::dynamic_truth_table tt1(var_index);

    tt0 = kitty::shrink_to(kitty::cofactor0(tt, var_index), var_index);
    tt1 = kitty::shrink_to(kitty::cofactor1(tt, var_index), var_index);

    /*--computing probability gate---*/
    auto c0_ones = kitty::count_ones(tt0);
    auto c1_ones = kitty::count_ones(tt1);
    auto tt_ones = kitty::count_ones(tt);
    bool is_const = 0;
    auto it0 = std::find(zero_lines.begin(), zero_lines.end(), var_index);
    auto it1 = std::find(one_lines.begin(), one_lines.end(), var_index);
    if ( it1 != one_lines.end() ) // insert not gate
    {
        if( gates.find(var_index)==gates.end() )
            gates[var_index].emplace_back(std::pair{M_PI, std::vector<uint32_t>{}});
        is_const = 1;
    }
    else if (it0 != zero_lines.end()) // inzert zero gate
    {
        is_const = 1;
    }
    else if (c0_ones != tt_ones)
    { /* == --> identity and ignore */
        double angle = 2 * acos(sqrt(static_cast<double>(c0_ones) / tt_ones));
        //angle *= (180/3.14159265); //in degree
        /*----add probability gate----*/

        gates[var_index].emplace_back(std::pair{angle, controls});
    }

    /*-----qc of cofactors-------*/
    /*---check state---*/
    auto c0_allone = (c0_ones == pow(2, tt0.num_vars())) ? true : false;
    auto c0_allzero = (c0_ones == 0) ? true : false;
    auto c1_allone = (c1_ones == pow(2, tt1.num_vars())) ? true : false;
    auto c1_allzero = (c1_ones == 0) ? true : false;

    std::vector<uint32_t> controls_new0;
    std::copy(controls.begin(), controls.end(), back_inserter(controls_new0));
    if (!is_const)
    {
        auto ctrl0 = var_index * 2 + 1; /* negetive control: /2 ---> index %2 ---> sign */
        controls_new0.emplace_back(ctrl0);
    }
    std::vector<uint32_t> controls_new1;
    std::copy(controls.begin(), controls.end(), back_inserter(controls_new1));
    if (!is_const)
    {
        auto ctrl1 = var_index * 2 + 0; /* positive control: /2 ---> index %2 ---> sign */
        controls_new1.emplace_back(ctrl1);
    }

    if (c0_allone)
    {
        /*---add H gates---*/
        for (auto i = 0u; i < var_index; i++)
            gates[i].emplace_back(std::pair{M_PI / 2, controls_new0});
        /*--check one cofactor----*/
        if (c1_allone)
        {
            /*---add H gates---*/
            for (auto i = 0u; i < var_index; i++)
                gates[i].emplace_back(std::pair{M_PI / 2, controls_new1});
        }
        else if (c1_allzero)
        {
            return;
        }
        else
        { /* some 1 some 0 */
            MC_qg_generation(gates, tt1, var_index - 1, controls_new1, zero_lines, one_lines);
        }
    }
    else if (c0_allzero)
    {
        /*--check one cofactor----*/
        if (c1_allone)
        {
            /*---add H gates---*/
            for (auto i = 0u; i < var_index; i++)
                gates[i].emplace_back(std::pair{M_PI / 2, controls_new1});
        }
        else if (c1_allzero)
        {
            return;
        }
        else
        { /* some 1 some 0 */
            MC_qg_generation(gates, tt1, var_index - 1, controls_new1, zero_lines, one_lines);
        }
    }
    else
    { /* some 0 some 1 for c0 */
        if (c1_allone)
        {
            MC_qg_generation(gates, tt0, var_index - 1, controls_new0, zero_lines, one_lines);
            /*---add H gates---*/
            for (auto i = 0u; i < var_index; i++)
                gates[i].emplace_back(std::pair{M_PI / 2, controls_new1});
        }
        else if (c1_allzero)
        {
            MC_qg_generation(gates, tt0, var_index - 1, controls_new0, zero_lines, one_lines);
        }
        else
        { /* some 1 some 0 */
            MC_qg_generation(gates, tt0, var_index - 1, controls_new0, zero_lines, one_lines);
            MC_qg_generation(gates, tt1, var_index - 1, controls_new1, zero_lines, one_lines);
        }
    }
}

/* with dependencies */
void gates_statistics(gates_t gates, std::map<uint32_t, bool> const &have_dependencies,
                      uint32_t const num_vars, qsp_1bench_stats &stats)
{
    auto total_rys = 0;
    auto total_cnots = 0;
    auto total_nots = 0;
    bool have_max_controls;
    auto n_reduc = 0; /* lines that always are zero or one and so we dont need to prepare them */

    for (int32_t i = num_vars - 1; i >= 0; i--)
    {
        auto rys = 0;
        auto cnots = 0;
        auto nots = 0;
        have_max_controls = 0;

        if (gates.find(i) == gates.end())
        {
            n_reduc++;
            continue;
        }
        if (gates[i].size() == 0)
        {
            n_reduc++;
            continue;
        }
        if (gates[i].size() == 1 && gates[i][0].first == M_PI && gates[i][0].second.size() == 0)
        {
            total_nots++;
            n_reduc++;
            continue;
        }

        /* check deps */
        auto it = have_dependencies.find(i);
        /* there exists deps */
        if (it != have_dependencies.end())
        {
            for (auto j = 0u; j < gates[i].size(); j++)
            {
                if (gates[i][j].second.size() == ((num_vars - i - 1) - n_reduc) &&
                    gates[i][j].second.size() != 0) /* number of controls is max or not? */
                {
                    have_max_controls = 1;
                    break;
                }

                auto cs = gates[i][j].second.size();
                if (cs == 0 && (std::abs(gates[i][j].first - M_PI) < 0.1))
                    nots++;
                else if (cs == 0)
                    rys++;
                else if (cs == 1 && (std::abs(gates[i][j].first - M_PI) < 0.1))
                    cnots++;
                else
                {
                    rys += pow(2, cs);
                    cnots += pow(2, cs);
                }
            }
            if (have_max_controls || cnots > pow(2, ((num_vars - i - 1) - n_reduc))) /* we have max number of controls */
            {
                if (i == int(num_vars - 1 - n_reduc)) /* first line for preparation */
                {
                    cnots = 0;
                    rys = 1;
                }
                else if (gates[i].size() == 1 && (std::abs(gates[i][0].first - M_PI) < 0.1) && gates[i][0].second.size() == 1) // second line for preparation
                {
                    cnots = 1;
                    rys = 0;
                }
                else /* other lines with more than one control */
                {
                    rys = pow(2, ((num_vars - i - 1) - n_reduc));
                    cnots = pow(2, ((num_vars - i - 1) - n_reduc));
                }
            }         
        }
        /* doesn't exist deps */
        else
        {
            for (auto j = 0u; j < gates[i].size(); j++)
            {
                if (gates[i][j].second.size() == ((num_vars - i - 1) - n_reduc) &&
                    gates[i][j].second.size() != 0) /* number of controls is max or not? */
                {
                    have_max_controls = 1;
                    break;
                }

                auto cs = gates[i][j].second.size();
                if (cs == 0 && (std::abs(gates[i][j].first - M_PI) < 0.1))
                    nots++;
                else if (cs == 0)
                    rys++;
                else if (cs == 1 && (std::abs(gates[i][j].first - M_PI) < 0.1))
                    cnots += 1;
                else
                {
                    rys += pow(2, cs);
                    cnots += pow(2, cs);
                }
            }
            if (have_max_controls || cnots > pow(2, ((num_vars - i - 1) - n_reduc))) /* we have max number of controls */
            {
                if (i == int(num_vars - 1 - n_reduc)) /* first line for preparation */
                {
                    cnots = 0;
                    rys = 1;
                }
                else if (gates[i].size() == 1 && (std::abs(gates[i][0].first - M_PI) < 0.1) && gates[i][0].second.size() == 1) // second line for preparation
                {
                    cnots = 1;
                    rys = 0;
                }
                else /* other lines with more than one control */
                {
                    rys = pow(2, ((num_vars - i - 1) - n_reduc));
                    cnots = pow(2, ((num_vars - i - 1) - n_reduc));
                }
            }
        }

        total_rys += rys;
        total_cnots += cnots;
        total_nots += nots;
    }

    stats.total_cnots += total_cnots;
    stats.total_rys += total_rys;
    stats.total_nots += total_nots;
    stats.gates_count = std::make_pair(total_cnots, total_rys + total_nots);

    if (have_dependencies.size() > 0)
    {
        if (total_cnots < (pow(2, num_vars - n_reduc) - 2))
        {
            //++stats.funcdep_bench_useful;
        }
        else
        {
            //++stats.funcdep_bench_notuseful;
        }
    }

    return;
}

/* without dependencies */
void gates_statistics(gates_t gates, uint32_t const num_vars, qsp_1bench_stats &stats)
{
    auto total_rys = 0;
    auto total_cnots = 0;
    auto total_nots = 0;
    bool have_max_controls;
    auto n_reduc = 0; /* lines that always are zero or one and so we dont need to prepare them */

    for (int32_t i = num_vars - 1; i >= 0; i--)
    {
        auto rys = 0;
        auto cnots = 0;
        auto nots = 0;
        have_max_controls = 0;

        if (gates.find(i) == gates.end())
        {
            n_reduc++;
            continue;
        }
        if (gates[i].size() == 0)
        {
            n_reduc++;
            continue;
        }
        if (gates[i].size() == 1 && gates[i][0].first == M_PI && gates[i][0].second.size() == 0)
        {
            nots++;
            n_reduc++;
            continue;
        }

        for (auto j = 0u; j < gates[i].size(); j++)
        {
            if (gates[i][j].second.size() == ((num_vars - i - 1) - n_reduc) &&
                gates[i][j].second.size() != 0) /* number of controls is max or not? */
            {
                have_max_controls = 1;
                break;
            }

            auto cs = gates[i][j].second.size();
            if (cs == 0 && (std::abs(gates[i][j].first - M_PI) < 0.1))
                nots++;
            else if (cs == 0)
                rys++;
            else if (cs == 1 && (std::abs(gates[i][j].first - M_PI) < 0.1))
                cnots += 1;
            else
            {
                rys += pow(2, cs);
                cnots += pow(2, cs);
            }
        }
        if (have_max_controls || cnots > pow(2, ((num_vars - i - 1) - n_reduc))) /* we have max number of controls */
        {
            if (i == int(num_vars - 1 - n_reduc)) /* first line for preparation */
            {
                cnots = 0;
                rys = 1;
            }
            else if (gates[i].size() == 1 && (std::abs(gates[i][0].first - M_PI) < 0.1) && gates[i][0].second.size() == 1) // second line for preparation
            {
                cnots = 1;
                rys = 0;
            }
            else /* other lines with more than one control */
            {
                rys = pow(2, ((num_vars - i - 1) - n_reduc));
                cnots = pow(2, ((num_vars - i - 1) - n_reduc));
            }
        }

        total_rys += rys;
        total_cnots += cnots;
        total_nots += nots;
    }

    stats.total_cnots += total_cnots;
    stats.total_rys += total_rys;
    stats.total_nots += total_nots;
    stats.gates_count = std::make_pair(total_cnots, total_rys + total_nots);

    return;
}

template <class DependencyAnalysisAlgorithm>
void qsp(gates_t& qc_gates, kitty::dynamic_truth_table tt, std::vector<order_t> orders, qsp_1bench_stats& final_qsp_stats)
{
    const uint32_t qubits_count = tt.num_vars();
    auto max_cnots = pow(2, qubits_count + 1);
    order_t best_order(qubits_count);
    qsp_1bench_stats best_stats;
    gates_t best_gates;
    stopwatch<>::duration_type time_traversal{0};
    {
        stopwatch t(time_traversal);
        for (auto order : orders)
        {
            kitty::dynamic_truth_table tt_copy = tt;
            angel::reordering_on_tt_inplace(tt_copy, order);
            //kitty::print_binary(tt_copy);
            //std::cout<<std::endl;
            qsp_1bench_stats qsp_stats;
            
            typename DependencyAnalysisAlgorithm::parameter_type pt;
            typename DependencyAnalysisAlgorithm::statistics_type st;
            auto result_deps = compute_dependencies<DependencyAnalysisAlgorithm>(tt_copy, pt, st);
            //result_deps.print();
            
            std::map<uint32_t, bool> have_deps;
             for(auto i=0u; i<qubits_count; i++)
            {
                if(result_deps.dependencies.find(i) != result_deps.dependencies.end())
                {
                    have_deps[i] = true;
                }
            }
            
            std::vector<uint32_t> zero_lines;
            std::vector<uint32_t> one_lines;
            extract_independent_vars(zero_lines, one_lines, tt_copy);

            gates_t gates;
            auto var_idx = qubits_count - 1;
            std::vector<uint32_t> cs;

            if ( !result_deps.dependencies.empty() )
            {
                MC_qg_generation(gates, qubits_count, tt_copy, var_idx, cs, result_deps.dependencies, zero_lines, one_lines);
                gates_statistics(gates, have_deps, qubits_count, qsp_stats);
            }
            else
            {
                MC_qg_generation(gates, tt_copy, var_idx, cs, zero_lines, one_lines);
                gates_statistics(gates, have_deps, qubits_count, qsp_stats);
            }

            if (qsp_stats.total_cnots < max_cnots)
            {
                std::copy(order.begin(), order.end(), best_order.begin());
                best_stats = qsp_stats;
                best_gates.clear();
                best_gates = gates;
                max_cnots = qsp_stats.total_cnots;
            }
        }
    }

    //extract_deps_operation_stats(op_stats, best_deps);

    qc_gates = best_gates;
    final_qsp_stats = best_stats;
    final_qsp_stats.total_time = time_traversal;
    //final_qsp_stats.total_bench += 1;
    //final_qsp_stats.has_no_dependencies += best_stats.has_no_dependencies;
    //final_qsp_stats.no_dependencies_computed += best_stats.no_dependencies_computed;
    //final_qsp_stats.has_dependencies += best_stats.has_dependencies;
    //final_qsp_stats.funcdep_bench_useful += best_stats.funcdep_bench_useful;
    //final_qsp_stats.funcdep_bench_notuseful += best_stats.funcdep_bench_notuseful;
    //final_qsp_stats.total_cnots = best_stats.total_cnots;
    //final_qsp_stats.total_rys = best_stats.total_rys;
    //final_qsp_stats.total_nots = best_stats.total_nots;
    //final_qsp_stats.gates_count = best_stats.gates_count;
      

}

template <class DependencyAnalysisAlgorithm>
void qsp_smallest_tt(gates_t& qc_gates, kitty::dynamic_truth_table tt, qsp_1bench_stats& final_qsp_stats)
{
    const uint32_t qubits_count = tt.num_vars();
    auto max_cnots = pow(2, qubits_count + 1);
    
    stopwatch<>::duration_type time_traversal{0};
    {
        stopwatch t(time_traversal);
        auto const [tt_min, phase, order] = kitty::exact_p_canonization(tt);
        
        typename DependencyAnalysisAlgorithm::parameter_type pt;
        typename DependencyAnalysisAlgorithm::statistics_type st;
        auto result_deps = compute_dependencies<DependencyAnalysisAlgorithm>(tt_min, pt, st);
        
        std::map<uint32_t, bool> have_deps;
        for(auto i=0u; i<qubits_count; i++)
        {
            if(result_deps.dependencies.find(i) != result_deps.dependencies.end())
            {
                have_deps[i] = true;
            }
        }
        
        std::vector<uint32_t> zero_lines;
        std::vector<uint32_t> one_lines;
        extract_independent_vars(zero_lines, one_lines, tt_min);

        auto var_idx = qubits_count - 1;
        std::vector<uint32_t> cs;

        if ( !result_deps.dependencies.empty() )
        {
            MC_qg_generation(qc_gates, qubits_count, tt_min, var_idx, cs, result_deps.dependencies, zero_lines, one_lines);
            gates_statistics(qc_gates, have_deps, qubits_count, final_qsp_stats);
        }
        else
        {
            MC_qg_generation(qc_gates, tt_min, var_idx, cs, zero_lines, one_lines);
            gates_statistics(qc_gates, have_deps, qubits_count, final_qsp_stats);
        }

    }

    final_qsp_stats.total_time = time_traversal;
    //final_qsp_stats.total_bench += 1;
}

/**
 * \breif General quantum state preparation algorithm for any function represantstion, 
 * dependency analysis algorithm, and reordering algorithm.
 * 
 * \tparam Network the type of generated quantum circuit
 * \tparam DependencyAnalysisAlgorithm specifies the method of extracting dependencies. By default, we don't have any dependencies.
 * \tparam ReorderingAlgorithm the way of extracting variable orders
 * \param net the extracted quantum circuit for given quantum state
 * \param tt given Boolean function corresponding to the quantum state
 * \param qsp_stats store all desired statistics of quantum state preparation process
*/
template <class Network, class DependencyAnalysisAlgorithm, class ReorderingAlgorithm>
void qsp_tt_general(Network &net, /*DependencyAnalysisAlgorithm deps_alg,*/ ReorderingAlgorithm orders_alg,
                    kitty::dynamic_truth_table tt, qsp_general_stats& final_qsp_stats /*, deps_operation_stats& op_stats*/)
{
    if(kitty::is_const0(tt))
    {
        std::cout<<"The tt is zero, algorithm return an empty circuit!\n";
        return;
    }
    const uint32_t qubits_count = tt.num_vars();
    for (auto i = 0u; i < qubits_count; i++)
        net.add_qubit();

    final_qsp_stats.total_bench ++;

    qsp_1bench_stats qsp_stats;

    /* caching */
    bool exist_in_cache = false;
    kitty::dynamic_truth_table tt_p_min;
    stopwatch<>::duration_type caching_time{0};
    {
        stopwatch t(caching_time);
        auto const [tt_min, phase, order] = kitty::exact_p_canonization(tt);
        tt_p_min = tt_min;
        if(final_qsp_stats.cache.find(tt_min._bits[0u]) != final_qsp_stats.cache.end()) /// already exist
        {   
            qsp_stats = final_qsp_stats.cache[tt_min._bits[0u]];
            exist_in_cache = true;
        }

    }
    if(exist_in_cache)
    {
        final_qsp_stats.total_cnots += qsp_stats.total_cnots; 
        final_qsp_stats.total_rys += qsp_stats.total_rys; 
        final_qsp_stats.total_nots += qsp_stats.total_nots; 
        final_qsp_stats.gates_count.emplace_back(qsp_stats.gates_count);
        final_qsp_stats.total_time += caching_time;
        return;
    }
    
    /* doing qsp */
    auto orders = orders_alg.run(qubits_count);
    gates_t qc_gates;
    qsp<DependencyAnalysisAlgorithm>(qc_gates, tt, orders, qsp_stats);  
    //qsp_smallest_tt<DependencyAnalysisAlgorithm>(qc_gates, tt, final_qsp_stats);

    /* update qsp stats */
    final_qsp_stats.total_cnots += qsp_stats.total_cnots; 
    final_qsp_stats.total_rys += qsp_stats.total_rys; 
    final_qsp_stats.total_nots += qsp_stats.total_nots; 
    final_qsp_stats.gates_count.emplace_back(qsp_stats.gates_count);
    final_qsp_stats.total_time += qsp_stats.total_time;
    final_qsp_stats.total_time += caching_time;

    /* insert into cache */
    final_qsp_stats.cache[tt_p_min._bits[0u]] = qsp_stats;
    
}

} // namespace angel
