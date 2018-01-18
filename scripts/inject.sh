pid=$1
INJECTLIB=$2

gdb --batch-silent -n -iex="set auto-solib-add off" -p $pid \
        --eval-command="sharedlibrary libc.so" \
        --eval-command="call (void) __libc_dlopen_mode(\"$INJECTLIB\", 0x80000000 | 0x002)" \
        --eval-command="sharedlibrary libglc-hook" \
        --eval-command="call (void) init_glc()" \
        --eval-command="detach"