# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(cache-read) begin
(cache-read) end
cache-read: exit(0)
EOF
pass;
