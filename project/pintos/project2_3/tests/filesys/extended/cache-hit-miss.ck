# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(cache-hit-miss) begin
(cache-hit-miss) end
cache-hit-miss: exit(0)
EOF
pass;
