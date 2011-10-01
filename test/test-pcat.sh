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

# Test that files that don't end with a newline get one
for ((f=0; f < $nfiles; f++)); do
    tr -cd '[:alnum:]' < /dev/urandom | dd bs=1 count=$RANDOM 2>/dev/null >> "${files[$f]}"
    [[ `tail -c1 "${files[$f]}"` != '\n' ]]     # Just to be sure ;)
done
"$pcat" "${files[@]}" > "$output"
for ((f=0; f < $nfiles; f++)); do
    echo >> "${files[$f]}"
done
cmp <(sort "${files[@]}") <(sort "$output") 

# Test that lines from any particular file maintain their original order.
for ((f=0; f < $nfiles; f++)); do
    sed -i "s/^/==$f== /" "${files[$f]}"     # Assumes '=' does not already occur in files
done
"$pcat" "${files[@]}" > "$output"
cmp <(sort "${files[@]}") <(sort "$output") 
for ((f=0; f < $nfiles; f++)); do
    cmp <(grep "^==$f==" "$output") "${files[$f]}" 
done

# Test really long lines, and binary zeros.
nlines=5
for ((n=0 ; n < $nlines ; n++)); do 
    dd if=/dev/zero bs=1k count=$RANDOM 2>/dev/null 
    dd if=/dev/zero bs=1  count=$RANDOM 2>/dev/null     # Want to get a length that's not always multiple of blocksize, but bs=1 is slooow
    echo 
done > "${files[0]}"
"$pcat" "${files[0]}" > "$output"
cmp <(sort "${files[0]}") <(sort "$output")


# Clean up.
rm "${files[@]}" "$output"

echo OK.
