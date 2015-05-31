/// Parameter.hpp
/// Shaun Harker
/// 2015-05-24

#ifndef DSGRN_PARAMETER_HPP
#define DSGRN_PARAMETER_HPP

#ifndef INLINE_IF_HEADER_ONLY
#define INLINE_IF_HEADER_ONLY
#endif

#include "Parameter.h"

INLINE_IF_HEADER_ONLY Parameter::
Parameter ( void ) { 
  data_ . reset ( new Parameter_ );
}

INLINE_IF_HEADER_ONLY Parameter::
Parameter ( std::vector<LogicParameter> const& logic,
            std::vector<OrderParameter> const& order, 
            Network const& network ) {
  assign ( logic, order, network );
}

INLINE_IF_HEADER_ONLY Parameter::
Parameter ( Network const& network ) {
  assign ( network );
}

INLINE_IF_HEADER_ONLY void Parameter::
assign ( std::vector<LogicParameter> const& logic,
         std::vector<OrderParameter> const& order, 
         Network const& network ) {
  data_ . reset ( new Parameter_ );
  data_ -> logic_ = logic;
  data_ -> order_ = order;
  data_ -> network_ = network;
}

INLINE_IF_HEADER_ONLY void Parameter::
assign ( Network const& network ) {
  data_ . reset ( new Parameter_ );
  data_ -> network_ = network;
}

  
INLINE_IF_HEADER_ONLY bool Parameter::
attracting ( Domain const& dom ) const {
  int D = data_ -> network_ . size ();
  for ( int d = 0; d < D; ++ d ) {
    if ( not dom . isMin(d) && absorbing ( dom, d, -1 ) ) return false;
    if ( not dom . isMax(d) && absorbing ( dom, d, 1 ) ) return false;
  }
  return true;
}

INLINE_IF_HEADER_ONLY bool Parameter::
absorbing ( Domain const& dom, int collapse_dim, int direction ) const {
  //std::cout << "Absorbing (" << dom . index () << ", " << collapse_dim << ", " << direction << ")\n";
  int thres = dom [ collapse_dim ];
  if ( direction == -1 ) thres -= 1;
  //std::cout << "  Threshold # = " << thres << "\n";
  int outedge = data_ -> order_ [ collapse_dim ] ( thres );
  //std::cout << "  This corresponds to the " << outedge << "th out-edge of node " << collapse_dim << "\n";
  int switching_dim = data_ -> network_ . outputs ( collapse_dim ) [ outedge ];
  //std::cout << "  This edge points to node " << switching_dim << " (i.e. this is the switching dimension.)\n";
  std::vector<bool> input_combination;
  //std::cout << "  Forming input combination by analyzing inputs of node " << collapse_dim << ".\n";
  for ( int source : data_ -> network_ . inputs ( collapse_dim ) ) {
    //std::cout << "    Analyze source edge " << source << "\n";
    bool activating = data_ -> network_ . interaction ( source, collapse_dim );
    //std::cout << "      This edge is " << (activating ? "activating" : "repressing" ) << ".\n";
    int inedge = data_ -> network_ . order ( source, collapse_dim );
    //std::cout << "      This edge is the " << inedge << "th ordered outedge of " << source << ".\n";
    int thres = data_ -> order_ [ source ] . inverse ( inedge );
    //std::cout << "      The input combination digit depends on which side of threshold " << thres << " on dimension " << source << " we are at.\n";
    bool result = not ( dom [ source ] > thres ) ^ activating;
    //std::cout << "      The domain is on the " << ( ( dom [ source ] > thres ) ? "right" : "left" ) << " side of this threshold.\n";
    input_combination . push_back ( result );
    //std::cout << "      Hence, the input combination digit is " << (result ? "1" : "0") << "\n";
  }
  //std::cout << "  Input combination formed. Big-endian representation = ";
  //for ( int i = input_combination . size () - 1; i >= 0; -- i ){ 
  // std::cout << (input_combination[i] ? "1" : "0");
  //} 
  //std::cout << "\n";
  //std::cout << "  Consulting parameter " <<  data_ -> logic_ [ collapse_dim ] . stringify () << ".\n";
  bool flow_direction = data_ -> logic_ [ collapse_dim ] ( input_combination, thres );
  //std::cout << "  Flow direction is to the " << (flow_direction ? "right" : "left") << "\n";
  if ( direction == -1 ) {
    //std::cout << "  Hence the left wall is " << ((flow_direction)?"not ":"") << "absorbing.\n";
    return not flow_direction;
  } else {
    //std::cout << "  Hence the right wall is " << ((not flow_direction)?"not ":"") << "absorbing.\n";
    return flow_direction;
  }
}

INLINE_IF_HEADER_ONLY Network const Parameter::
network ( void ) const {
  return data_ -> network_;
}

INLINE_IF_HEADER_ONLY std::string Parameter::
stringify ( void ) const {
  std::stringstream ss;
  uint64_t D = data_ -> network_ . size ();
  ss << "[";
  for ( uint64_t d = 0; d < D; ++ d ) {
    if ( d > 0 ) ss << ",";
    ss << "[\"" << network() . name ( d ) << "\"," 
       << data_ -> logic_[d] <<","<< data_ -> order_[d] << "]";
  }
  ss << "]";
  return ss . str ();
}

INLINE_IF_HEADER_ONLY void Parameter::
parse ( std::string const& str ) {
  std::shared_ptr<JSON::Array> json = JSON::toArray(JSON::parse(str));
  data_ -> logic_ . clear ();
  data_ -> order_ . clear ();
  for ( auto value : *json ) {
    data_ -> logic_ . push_back ( LogicParameter () );
    data_ -> order_ . push_back ( OrderParameter () );
    std::shared_ptr<JSON::Array> node = toArray(value);
    // (*node)[0] has node name, which we ignore.
    data_ -> logic_ . back() . parse ( JSON::stringify ( (*node)[1] ));
    data_ -> order_ . back() . parse ( JSON::stringify ( (*node)[2] ));
  }
}

INLINE_IF_HEADER_ONLY std::string Parameter::
inequalities ( void ) const {
  std::stringstream ss;
  uint64_t D = data_ -> network_ . size ();
  ss << "[";
  bool outerfirst = true;
  for ( uint64_t d = 0; d < D; ++ d ) {
    if ( outerfirst ) outerfirst = false; else ss << ",\n";
    std::string node_name = network() . name ( d );
    uint64_t n = network() . inputs ( d ) . size ();
    uint64_t m = network() . outputs ( d ) . size ();
    uint64_t N = ( 1LL << n );
    auto input_combo_string = [&](uint64_t i) {
      std::stringstream input_ss;
      std::vector<std::vector<uint64_t>> logic = 
        network () . logic ( d );
      uint64_t bit = 1;
      uint64_t k = 0;
      for ( auto const& factor : logic ) {
        if ( factor . size () > 1 ) input_ss << "(";
        bool inner_first = true;
        for ( uint64_t source : factor ) {
          if ( inner_first ) inner_first = false; else input_ss << " + ";
          std::string source_name = network() . name(source);
          if ( i & bit ) {
            input_ss << "U(" << source_name <<"," << node_name << ")";
          } else {
            input_ss << "L(" << source_name <<"," << node_name << ")";
          }
          bit <<= 1;
          ++ k;
        }
        if ( factor . size () > 1 ) input_ss << ")"; 
        else if ( k < n ) input_ss << " ";
      }
      return input_ss . str ();
    };
    auto output_string = [&](uint64_t j) {
      uint64_t target = network() . outputs ( d ) [ j ];
      std::string target_name = network() . name(target);
      std::stringstream output_ss;
      output_ss << "THETA(" << node_name << "," << target_name << ")";
      return output_ss . str ();
    };
    ss << "\"{\n";
    bool first = true;
    for ( uint64_t i = 0; i < N; ++ i ) {
      if ( first ) first = false; else ss << ",\n";
      uint64_t j = 0;
      while ( j < m && data_ -> logic_[d] ( m*i + j ) ) ++ j;
      if ( j == 0 ) {
        ss << input_combo_string ( i ) << " < " << output_string (0);
      } else if ( j == m ) {
        ss << output_string (m-1) << " < " << input_combo_string ( i );
      } else {
        ss << output_string (j-1) << " < " 
           << input_combo_string ( i ) << " < " 
           << output_string (j);
      }
    }
    ss << "\n},{\n";
    first = true;
    for ( uint64_t j = 0; j < m; ++ j ) {
      if ( first ) first = false; else ss << " < ";
      ss << output_string ( data_ -> order_[d](j) );
    }
    ss << "\n}\"";
  }
  ss << "]";
  return ss . str ();
}

INLINE_IF_HEADER_ONLY std::ostream& operator << ( std::ostream& stream, Parameter const& p ) {
  stream << p.stringify();
  return stream;
}

#endif