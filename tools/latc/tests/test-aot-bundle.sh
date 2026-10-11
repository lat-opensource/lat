#!/bin/sh
set -eu

latc=$1
guest=$2
aot=$3
bundle=$4

"$latc" compile "$guest" -o "$bundle" --runner "$guest" --aot "$aot"
"$latc" inspect --json "$bundle" | python3 -c \
    'import json,sys; d=json.load(sys.stdin); assert d["execution_model"] == "lat-aot-bundle", d; assert d["aot_size"] > 0, d; assert d["aot_name"] == "fake.aot2", d'
echo "test-aot-bundle: PASS"
