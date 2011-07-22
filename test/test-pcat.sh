#!/bin/bash

# Functional tests for pcat.

# Copyright 2011 John Kleint 
# This is free software, licensed under the GNU General Public License v3,
# available in the accompanying file LICENSE.txt.

nfiles=16
max_line_len=$((32 * 1024))
set -e
export LC_ALL=C
bindir=$(dirname $(readlink -f "${BASH_SOURCE[0]}"))/../bin
pcat="$bindir/pcat"
output="$(tempfile -p pcato || exit 1)"

# Make some text files of random strings.
declare -a files
declare -a pipes
for ((f=0; f < $nfiles; f++)); do
    files[$f]=$(tempfile -p pcat. || exit 1)
    pipes[$f]="<(cat ${files[$f]})"     
    tr -cd '[:alnum:]_\n' < /dev/urandom | head -$RANDOM > "${files[$f]}" 
done

# Test single file on stdin
"$pcat" < "${files[0]}" > "$output" 
diff "${files[0]}" "$output" 

# Test all files on pipe to stdin
cat "${files[@]}" | "$pcat" > "$output"
diff <(cat "${files[@]}") "$output"

# Test file and pipe combo
"$pcat" "${files[0]}" <(cat "${files[1]}") > "$output"
diff <(sort "${files[0]}" "${files[1]}") <(sort "$output")

# pcat files, f at a time, and make sure we get everything
for ((f=1; f < $nfiles ; f++)); do
    "$pcat" "${files[@]:0:$f}" > "$output"
    diff <(sort "${files[@]:0:$f}") <(sort "$output")
done

# pcat files as pipes, f at a time, and make sure we get everything
for ((f=1; f < $nfiles ; f++)); do
    eval "$pcat" "${pipes[@]:0:$f}" > "$output"        # This seems a little sketchy
    diff <(sort "${files[@]:0:$f}") <(sort "$output")
done

# Test lines of all lengths.
# There's probably a faster/prettier way to do this.
for ((len=0; len < $max_line_len; len++)); do printf '%*s\n' $len; done | "$pcat" | gzip > "$output"
for ((len=0; len < $max_line_len; len++)); do printf '%*s\n' $len; done | "$pcat" | cmp - <(zcat "$output")

# TODO: Test reallyreally long lines.

# Clean up.
rm "${files[@]}" "$output"

echo OK.
