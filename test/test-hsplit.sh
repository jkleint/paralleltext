#!/bin/bash

# Functional tests for hsplit.

# Copyright 2011 John Kleint 
# This is free software, licensed under the GNU General Public License v3,
# available in the accompanying file LICENSE.txt.

nfiles=128      # Runtime is O(nfiles^2)
maxbins=8       # Needs to be <= nfiles; Runtime is O(maxbins^3)
nlines=100000
export LC_ALL=C
set -e
bindir=$(dirname $(readlink -f "${BASH_SOURCE[0]}"))/../bin
hsplit="$bindir/hsplit"
infile="$(tempfile -p hsplit || exit 1)"
sorted="$(tempfile -p hsplit || exit 1)"

# Generate some random strings for input. 
tr -cd '[:alnum:]_\n' < /dev/urandom | head -$nlines > "$infile"
sort "$infile" > "$sorted"

declare -a files
for ((f=0; f < $nfiles; f++)); do
    files[$f]="$(tempfile -p hsplit || exit 1)"
done

# splitting to one file should give us the original.
"$hsplit" "${files[0]}" < "$infile"
diff "$infile" "${files[0]}"

# Splitting to no files should give us integer hashcodes on stdout.
"$hsplit" < "$infile" > "${files[0]}" 
[[ ! $(egrep -v '^[0-9]+$' "${files[0]}") ]]

# Appending to existing files should not overwrite them
head -$(($nlines / 2)) "$infile" | "$hsplit" "${files[@]:0:2}"
tail -n+$(($nlines / 2 + 1)) "$infile" | "$hsplit" -a "${files[@]:0:2}"
cmp "$sorted" <(sort "${files[@]:0:2}")

# No two output files should contain the same line.
for ((bins=2; bins < $maxbins; bins++)); do
    "$hsplit" "${files[@]:0:$bins}" < "$infile"
    for ((i=0 ; i < $bins; i++)); do
        for ((j=$i+1 ; j < $bins; j++)); do
            [[ -z $(comm --nocheck-order -12 "${files[$i]}" "${files[$j]}") ]]
        done
    done
done

# Split to multiple files, make sure we got everything
for ((f=1; f < $nfiles; f++)); do
    "$hsplit" "${files[@]:0:$f}" < "$infile"
    # TODO: Would be better to check that output files maintain order; not sure how
    cmp "$sorted" <(sort "${files[@]:0:$f}")     
done

# Distribution of lines to files should be pretty even.
distlines=1000000
for ((bins=2; bins < $maxbins; bins++)); do
    wcs="$(printf '>(wc -l) %.0s' $(seq $bins))"        # Cheesy way to replicate a string $bins times
    sizes=( $(seq $distlines | eval "$hsplit" "$wcs" | sort -n) )
    min="${sizes[0]}"
    max="${sizes[$bins-1]}"
    # TODO: Figure out what the variation should be say 99.99% of the time.
    [[ $(($max - $min)) -lt $(($distlines / 100)) ]]    # Check that variation in bin sizes is < 1%.
done

# Clean up
rm "$infile" "$sorted" "${files[@]:0:$nfiles}"

echo OK.
