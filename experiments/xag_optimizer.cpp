/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined( BILL_HAS_Z3 )

#include "experiments.hpp"

#include <bill/sat/interface/abc_bsat2.hpp>
#include <bill/sat/interface/z3.hpp>
#include <fmt/format.h>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/print.hpp>
#include <mockturtle/algorithms/detail/minmc_xags.hpp>
#include <mockturtle/algorithms/exact_mc_synthesis.hpp>
#include <mockturtle/algorithms/xag_optimization.hpp>
#include <mockturtle/io/index_list.hpp>
#include <mockturtle/io/write_verilog.hpp>
#include <mockturtle/properties/mccost.hpp>
#include <mockturtle/utils/progress_bar.hpp>

#include <cstdint>
#include <string>






#include <caterpillar/caterpillar.hpp>
#include "caterpillar/synthesis/strategies/pebbling_mapping_strategy.hpp"
#include <caterpillar/details/utils.hpp>

#include <tweedledum/io/write_unicode.hpp>
#include <lorina/aiger.hpp>
#include <caterpillar/caterpillar.hpp>
#include <mockturtle/io/aiger_reader.hpp>

#include <mockturtle/networks/xag.hpp>





#include <iostream>
#include <kitty/static_truth_table.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/networks/xag.hpp>
#include <tweedledum/io/write_unicode.hpp>
#include <tweedledum/tweedledum.hpp>
#include <tweedledum/networks/netlist.hpp>


#include <lorina/aiger.hpp>
#include <mockturtle/algorithms/cleanup.hpp>
#include <mockturtle/algorithms/experimental/cost_generic_resub.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/utils/cost_functions.hpp>
#include <mockturtle/utils/stopwatch.hpp>



#include <mockturtle/algorithms/node_resynthesis/bidecomposition.hpp>
#include <mockturtle/algorithms/refactoring.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/aig.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <mockturtle/algorithms/node_resynthesis/xag_npn.hpp>
#include <mockturtle/algorithms/rewrite.hpp>
#include <mockturtle/io/aiger_reader.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/tech_library.hpp>
#include <mockturtle/views/depth_view.hpp>


int main()
{
  using namespace experiments;
  using namespace mockturtle;
  using namespace mockturtle::experimental;
  

  using namespace caterpillar;
  using namespace tweedledum;
  using namespace caterpillar::detail;
  using namespace std;
  using network_t = pebbling_view<xag_network>;

  experiment<std::string, uint32_t, uint32_t, double> exp( "exact_mc_synthesis", "name", "XOR gates", "AND gates", "total runtime" );

  // All spectral classes
  const auto all_spectral = [&]( uint32_t num_vars, bool multiple, bool minimize_xor, bool sat_linear_resyn ) {
    stopwatch<>::duration time{};
    uint32_t xor_gates{}, and_gates{};
    const auto prefix = fmt::format( "exact_mc_synthesis{}{}{}", multiple ? "-multiple" : "", minimize_xor ? "-xor" : "", sat_linear_resyn ? "-resyn" : "" );
    progress_bar pbar( mockturtle::detail::minmc_xags[num_vars].size(), prefix + " |{}| class = {}, function = {}, time so far = {:.2f}", true );
    for ( auto const& [cls, word, list, expr] : mockturtle::detail::minmc_xags[num_vars] )
    {
      (void)expr;
      stopwatch<> t( time );

      if ( word == 0u )
      {
        continue;
      }

      kitty::dynamic_truth_table tt( num_vars );
      kitty::create_from_words( tt, &word, &word + 1 );
      pbar( cls, cls, kitty::to_hex( tt ), to_seconds( time ) );

      exact_mc_synthesis_params ps;
      ps.break_symmetric_variables = true;
      ps.break_subset_symmetries = true;
      ps.break_multi_level_subset_symmetries = true;
      ps.ensure_to_use_gates = true;
      ps.auto_update_xor_bound = minimize_xor;
      ps.conflict_limit = 50000u;
      ps.ignore_conflict_limit_for_first_solution = true;

      xag_network xag;

      if ( multiple )
      {
        const auto xags = exact_mc_synthesis_multiple<xag_network, bill::solvers::z3>( tt, 50u, ps );
        xag = *std::min_element( xags.begin(), xags.end(), [&]( auto const& x1, auto const& x2 ) { return x1.num_gates() < x2.num_gates(); } );
      }
      else
      {
        xag = exact_mc_synthesis<xag_network, bill::solvers::z3>( tt, ps );
      }

      if ( sat_linear_resyn )
      {
        xag = exact_linear_resynthesis_optimization<bill::solvers::z3>( xag, 500000u );
        xag = xag_constant_fanin_optimization(xag);
        xag = xag_dont_cares_optimization(xag);
      }

      const auto num_ands = *multiplicative_complexity( xag );
      const auto num_xors = xag.num_gates() - num_ands;

      xor_gates += num_xors;
      and_gates += num_ands;

      /* verify MC */
      const auto xag_db = create_from_binary_index_list<xag_network>( list.begin() );
      if ( *multiplicative_complexity( xag_db ) != *multiplicative_complexity( xag ) )
      {
        fmt::print( "[e] MC mismatch for {}, got {}, expected {}.\n", kitty::to_hex( tt ), *multiplicative_complexity( xag ), *multiplicative_complexity( xag_db ) );
        std::abort();
      }
    }
    const auto name = fmt::format( "all-spectral-{}{}{}{}", num_vars, multiple ? "-multiple" : "", minimize_xor ? "-xor" : "", sat_linear_resyn ? "-resyn" : "" );
    exp( name, xor_gates, and_gates, to_seconds( time ) );
  };

  // Confirm some 6-input functions with MC = 4
  const auto practical6 = [&]( bool multiple, bool minimize_xor, bool sat_linear_resyn ) {
    stopwatch<>::duration time{};
    uint32_t xor_gates{}, and_gates{};
    const auto prefix = fmt::format( "exact_mc_synthesis{}{}{}", multiple ? "-multiple" : "", minimize_xor ? "-xor" : "", sat_linear_resyn ? "-resyn" : "" );

    std::vector<uint64_t> functions = {
        /*UINT64_C( 0x6996966996696996 ),
        UINT64_C( 0x9669699669969669 ),
        UINT64_C( 0x0000000069969669 ),
        UINT64_C( 0x0000000096696996 ),
        UINT64_C( 0x9669699600000000 ),
        UINT64_C( 0x6996966900000000 ),
        UINT64_C( 0x1ee1e11ee11e1ee1 ),
        UINT64_C( 0xe11e1ee11ee1e11e ),
        UINT64_C( 0x6996699669969669 ),
        UINT64_C( 0x00000000e11e1ee1 ),
        UINT64_C( 0x6969699696969669 ),
        UINT64_C( 0x56a9a956a95656a9 ),
        UINT64_C( 0x000000001ee1e11e ),
        UINT64_C( 0x9669966996696996 ),
        UINT64_C( 0x9669966996690000 ),
        UINT64_C( 0x9696966969696996 ),
        UINT64_C( 0x00000000953f6ac0 ),
        UINT64_C( 0xa95656a956a9a956 ),
        UINT64_C( 0x0000000000006996 ),
        UINT64_C( 0x00000000ff00807f ),
        UINT64_C( 0x6669999699966669 ),
        UINT64_C( 0x0000000096969669 ),*/
        UINT64_C( 0x96665aaa3cccf000 )/*,
        UINT64_C( 0xe888a000c0000000 ),
        UINT64_C( 0x00000000153f55ff ),
        UINT64_C( 0xaa00aa0080000000 ),
        UINT64_C( 0xaa00aa00953f55ff ),
        UINT64_C( 0xc0c00000eac0aa00 ),
        UINT64_C( 0x9996666966699996 ),
        UINT64_C( 0x0000000069696996 ),
        UINT64_C( 0x00000000999a0000 ),
        UINT64_C( 0x0000ffff00006665 ),
        UINT64_C( 0xffff00006665999a ),
        UINT64_C( 0xe11e1ee100000000 ),
        UINT64_C( 0x0000000000009669 ),
        UINT64_C( 0x6996699669960000 ),
        UINT64_C( 0x00000000ffff6996 ),
        UINT64_C( 0x00000000a95656a9 ),
        UINT64_C( 0x2882822882282882 ),
        UINT64_C( 0x1ee1e11e00000000 ),
        UINT64_C( 0x9696966900000000 ),
        UINT64_C( 0x0000000099966669 ),
        UINT64_C( 0x0000000066699996 ),
        UINT64_C( 0x953f6ac06ac0953f ),
        UINT64_C( 0x0000000056a9a956 ),
        UINT64_C( 0x6ac0953f00000000 ),
        UINT64_C( 0x0000ffff00007fff ),
        UINT64_C( 0x6969699600000000 ),
        UINT64_C( 0xff00807f00ff7f80 ),
        UINT64_C( 0x00000000ffff9669 ),
        UINT64_C( 0x9996666900000000 ),
        UINT64_C( 0x0000699669960000 ),
        UINT64_C( 0x00000000aaaa5556 ),
        UINT64_C( 0x00ff7f8000000000 ),
        UINT64_C( 0x6669999600000000 ),
        UINT64_C( 0x9669000000009669 ),
        UINT64_C( 0xfdfdfd5400000000 ),
        UINT64_C( 0x0001111155555555 ),
        UINT64_C( 0x00000000fdfdfd54 ),
        UINT64_C( 0x0000ffff0000f0e1 ),
        UINT64_C( 0xfdfdfd54020202ab ),
        UINT64_C( 0x020202ab00000000 ),
        UINT64_C( 0xa95656a900000000 ),
        UINT64_C( 0xefeeaaaa00000000 ),
        UINT64_C( 0x00a9000000a900a9 ),
        UINT64_C( 0xa956a9a9a956a956 ),
        UINT64_C( 0x5600565656005600 ),
        UINT64_C( 0xa888a00000000000 ),
        UINT64_C( 0x0000222333333333 ),
        UINT64_C( 0x0000966996690000 ),
        UINT64_C( 0x0000ffff0000f096 ),
        UINT64_C( 0xfff1111100000000 ),
        UINT64_C( 0x000c000ccccf888e ),
        UINT64_C( 0xccc3ccc300004441 ),
        UINT64_C( 0xffff035703030303 ),
        UINT64_C( 0xccc3ccc3333c6669 ),
        UINT64_C( 0x333c333c00001114 ),
        UINT64_C( 0x888288828882ccc3 ),
        UINT64_C( 0x00000000ccc38882 ),
        UINT64_C( 0xa900a9a9a900a900 ),
        UINT64_C( 0xf1fff1f111111111 ),
        UINT64_C( 0x02ab020202ab02ab ),
        UINT64_C( 0xffcdcccc00000000 ),
        UINT64_C( 0x00000000566a0000 ),
        UINT64_C( 0x2223000233333333 ),
        UINT64_C( 0x0000ffff0000a995 ),
        UINT64_C( 0x00000000fff11111 ),
        UINT64_C( 0xaaffbeeb00aa2882 ),
        UINT64_C( 0xffff0000a995566a ),
        UINT64_C( 0x00000000ffcdcccc ),
        UINT64_C( 0x000eeeee00000000 ),
        UINT64_C( 0x55004114ff00c33c ),
        UINT64_C( 0x0032333300000000 ),
        UINT64_C( 0xfff11111000eeeee ),
        UINT64_C( 0xffcdcccc00323333 ),
        UINT64_C( 0x0000ffff00000f69 ),
        UINT64_C( 0x0000ffff00000f1e ),
        UINT64_C( 0x000000000000007f ),
        UINT64_C( 0x1115155500010111 ),
        UINT64_C( 0x0000ffff00006566 )*/};

    progress_bar pbar( static_cast<uint32_t>( functions.size() ), prefix + " |{}| function = {:016x}, time so far = {:.2f}", true );

    for ( auto i = 0u; i < functions.size(); ++i )
    {
      stopwatch<> t( time );

      kitty::dynamic_truth_table tt( 6u );
      kitty::create_from_words( tt, &functions[i], &functions[i] + 1 );

      if ( kitty::get_bit( tt, 0u ) )
      {
        tt = ~tt;
      }

      pbar( i, functions[i], to_seconds( time ) );
      
      exact_mc_synthesis_params ps;
      ps.verbose = true;
      ps.very_verbose = true;
      ps.break_symmetric_variables = true;
      ps.break_subset_symmetries = true;
      ps.break_multi_level_subset_symmetries = true;
      ps.ensure_to_use_gates = true;
      ps.auto_update_xor_bound = minimize_xor;
      ps.conflict_limit = 50000u;

      mockturtle::xag_network xag;
      mockturtle::xag_network xagi1;
      mockturtle::xag_network xagi11;
      mockturtle::xag_network xagi2;
      mockturtle::xag_network xagi3;
      mockturtle::xag_network xagi4;
       

      if ( multiple )
      {
        const auto xags = exact_mc_synthesis_multiple<xag_network, bill::solvers::z3>( tt, 5u, ps );
        xag = *std::min_element( xags.begin(), xags.end(), [&]( auto const& x1, auto const& x2 ) { return x1.num_gates() < x2.num_gates(); } );
      }
      else
      {
        xag = exact_mc_synthesis<xag_network, bill::solvers::z3>( tt, ps );
      }

      if ( sat_linear_resyn )
      {
        fmt::print( "Linear resynthesis-XOR reduction\n");
        
        const auto numands1 = *multiplicative_complexity( xag );
        const auto numxors1 = xag.num_gates() - numands1;
        fmt::print( "\n  XorsXAGI1 = {}\n\n", numxors1) ;
        fmt::print( "\n  AndsXAGI1 = {}\n\n", numands1 );
        
        xagi1=xag;
        xag = exact_linear_resynthesis_optimization( xag, 500000u );
        xag = cleanup_dangling( xag );
        
        fmt::print( " AND Reduction\n" );
        xag = xag_constant_fanin_optimization(xag);
        xag = cleanup_dangling( xag );
        
       
        xag = xag_dont_cares_optimization(xag);
        xag = cleanup_dangling( xag );
        xagi11=xag;
        const auto numands2 = *multiplicative_complexity( xag );
        const auto numxors2 = xag.num_gates() - numands2;
        fmt::print( "\n  XorsXAGI2 = {}\n\n", numxors2) ;
        fmt::print( "\n  AndsXAGI2 = {}\n\n", numands2 );
        
        
        auto costfn = t_xag_depth_cost_function<xag_network>();
        cost_generic_resub_params ss;
        cost_generic_resub_stats sts;
        ss.verbose = true;

        cost_generic_resub( xag, costfn, ss, &sts );
        xagi2 = cleanup_dangling( xag );
        xag = cleanup_dangling( xag );
        const auto numands3 = *multiplicative_complexity( xag );
        const auto numxors3 = xag.num_gates() - numands3;
        fmt::print( "\n  XorsXAGI3 = {}\n\n", numxors3) ;
        fmt::print( "\n  AndsXAGI3 = {}\n\n", numands3 );
        bidecomposition_resynthesis<xag_network> reyn;
        refactoring( xag, reyn );
        xag = cleanup_dangling( xag );
        xagi3 = xag;
        
        const auto numands4 = *multiplicative_complexity( xag );
        const auto numxors4 = xag.num_gates() - numands4;
        fmt::print( "\n  XorsXAGI4 = {}\n\n", numxors4) ;
        fmt::print( "\n  AndsXAGI4 = {}\n\n", numands4 );
        
        xag_npn_resynthesis<xag_network, xag_network, xag_npn_db_kind::xag_incomplete> resyn;
        
        exact_library_params eps;
        eps.np_classification = false;
        exact_library<xag_network, decltype( resyn )> exact_lib( resyn, eps );
        rewrite_params p;
        rewrite_stats s;
        rewrite( xag, exact_lib, p, &s );
        xagi4 = cleanup_dangling( xag );
        
        const auto numands5 = *multiplicative_complexity( xag );
        const auto numxors5 = xag.num_gates() - numands5;
        fmt::print( "\n  XorsXAGI5 = {}\n\n", numxors5) ;
        fmt::print( "\n  AndsXAGI5 = {}\n\n", numands5 );
        
        
      }

      

      //xor_gates += num_xors;
      //and_gates += num_ands;
      
      std::fstream FileName;                       
      FileName.open("XAGStruct/i1.qasm", ios::out);     
     
      network_t pxagi1 {xagi1};

      pebbling_mapping_strategy_params pssi1;
      pssi1.pebble_limit = 4;
      pebbling_mapping_strategy<network_t, z3_pebble_inplace_solver<network_t>> inplace_strategyi1 (pssi1);

  
      tweedledum::netlist<stg_gate> rneti1;
      logic_network_synthesis_stats sti1;
      logic_network_synthesis_params parami1;
      parami1.verbose = false;
 
      caterpillar::logic_network_synthesis(rneti1, pxagi1, inplace_strategyi1, {}, parami1, &sti1);
      //write_unicode(rnet);
      writeqasm(rneti1,"XAGStruct/i1.qasm");
      FileName.close();
      
      
      FileName.open("XAGStruct/i2.qasm", ios::out);     
     
      network_t pxagi2 {xagi11};

      pebbling_mapping_strategy_params pssi2;
      pssi2.pebble_limit = 4;
      pebbling_mapping_strategy<network_t, z3_pebble_inplace_solver<network_t>> inplace_strategyi2 (pssi2);

  
      tweedledum::netlist<stg_gate> rneti2;
      logic_network_synthesis_stats sti2;
      logic_network_synthesis_params parami2;
      parami2.verbose = false;
 
      caterpillar::logic_network_synthesis(rneti2, pxagi2, inplace_strategyi2, {}, parami2, &sti2);
      //write_unicode(rnet);
      writeqasm(rneti2,"XAGStruct/i2.qasm");
      FileName.close();
      
      FileName.open("XAGStruct/i3.qasm", ios::out);     
     
      network_t pxag {xagi2};

      pebbling_mapping_strategy_params pss;
      pss.pebble_limit = 4;
      pebbling_mapping_strategy<network_t, z3_pebble_inplace_solver<network_t>> inplace_strategy (pss);

  
      tweedledum::netlist<stg_gate> rnet;
      logic_network_synthesis_stats st;
      logic_network_synthesis_params param;
      param.verbose = false;
 
      caterpillar::logic_network_synthesis(rnet, pxag, inplace_strategy, {}, param, &st);
      //write_unicode(rnet);
      writeqasm(rnet,"XAGStruct/i3.qasm");
      FileName.close();
      
      
      FileName.open("XAGStruct/i4.qasm", ios::out);     
       
       
      
      network_t pxag4 {xagi3};

      pebbling_mapping_strategy_params pss4;
      pss4.pebble_limit = 4;
      pebbling_mapping_strategy<network_t, z3_pebble_inplace_solver<network_t>> inplace_strategyi4 (pss4);

  
      tweedledum::netlist<stg_gate> rnet4;
      logic_network_synthesis_stats st4;
      logic_network_synthesis_params param4;
      param.verbose = false;
 
      caterpillar::logic_network_synthesis(rnet4, pxag4, inplace_strategyi4, {}, param4, &st4);
      //write_unicode(rnet);
      writeqasm(rnet,"XAGStruct/i4.qasm");
      FileName.close();
      
      
      
      
      FileName.open("XAGStruct/i5.qasm", ios::out);     
     
      network_t pxag5 {xagi4};

      pebbling_mapping_strategy_params pss5;
      pss5.pebble_limit = 4;
      pebbling_mapping_strategy<network_t, z3_pebble_inplace_solver<network_t>> inplace_strategyi5 (pss5);

  
      tweedledum::netlist<stg_gate> rnet5;
      logic_network_synthesis_stats st5;
      logic_network_synthesis_params param5;
      param.verbose = false;
 
      caterpillar::logic_network_synthesis(rnet5, pxag5, inplace_strategyi5, {}, param5, &st5);
      //write_unicode(rnet);
      writeqasm(rnet,"XAGStruct/i5.qasm");
      FileName.close();
      
      
      
    }
    const auto name = fmt::format( "practical6{}{}{}", multiple ? "-multiple" : "", minimize_xor ? "-xor" : "", sat_linear_resyn ? "-resyn" : "" );
    exp( name, xor_gates, and_gates, to_seconds( time ) );
  };

  
  practical6( true, true, true );

  exp.save();
  exp.table();
}

#else

#include <iostream>

int main()
{
  std::cout << "requires Z3" << std::endl;
  return 0;
}

#endif
