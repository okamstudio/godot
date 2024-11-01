# Grisu2

This is a C++11 implementation of the Grisu2 algorithm for converting floating-point numbers to decimal strings.

The Grisu2 algorithm is by Florian Loitsch, based on the work of Robert G. Burger and R. Kent Dybvig:

- https://dl.acm.org/doi/10.1145/1806596.1806623 [1] Loitsch, "Printing Floating-Point Numbers Quickly and Accurately with Integers", Proceedings of the ACM SIGPLAN 2010 Conference on Programming Language Design and Implementation, PLDI 2010
- https://dl.acm.org/doi/10.1145/231379.231397 [2] Burger, Dybvig, "Printing Floating-Point Numbers Quickly and Accurately", Proceedings of the ACM SIGPLAN 1996 Conference on Programming Language Design and Implementation, PLDI 1996

The original C implementation is by Florian Loitsch:
- https://drive.google.com/file/d/0BwvYOx00EwKmejFIMjRORTFLcTA/view?resourcekey=0-1Lg8tXTC_JAODUcFpMcaTA

The implementation simplified and adapted to JSON and C++11 by Daniel Lemire as part of simdjson:
- https://github.com/simdjson/simdjson/blob/master/src/to_chars.cpp

The `grisu2.h` file is the same as `to_chars.cpp` but with `godot.patch` applied to it, with the following changes:
- Simplify namespaces to just be one `grisu2` namespace.
- Rename functions to ensure their names are unique.
- Make `to_chars` handle both float and double types instead of just double.
- Remove the trailing `.0` logic to match Godot's existing `String::num_scientific` behavior.
