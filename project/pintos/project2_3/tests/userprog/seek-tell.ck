# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(seek-tell) begin
(seek-tell) end
seek-tell: exit(0)
EOF
pass;
