#!/usr/bin/env python3
"""Apply the SETMAX patch to a pristine Redis 8.10.2 tree.

Usage: python3 apply_setmax.py <redis-source-dir> [--no-dirty]
"""
import os
import shutil
import sys

FRAGMENT = r'''
/* SETMAX key value
 *
 * Write `value` only if the key does not exist, or if its current integer
 * value is smaller. Returns 1 when the value is written, 0 otherwise. */
void setmaxCommand(client *c) {
    long long value;
    if (getLongLongFromObjectOrReply(c, c->argv[2], &value, NULL) != C_OK) return;

    dictEntryLink link;
    kvobj *o = lookupKeyWriteWithLink(c->db, c->argv[1], &link);
    if (checkType(c, o, OBJ_STRING)) return;

    if (o != NULL) {
        long long old;
        if (getLongLongFromObjectOrReply(c, o, &old, NULL) != C_OK) return;
        if (value <= old) {
            addReply(c, shared.czero);
            return;
        }
    }

    robj *new = createStringObjectFromLongLongForValue(value);
    if (o != NULL) {
        dbReplaceValueWithLink(c->db, c->argv[1], &new, link);
    } else {
        dbAddByLink(c->db, c->argv[1], &new, &link);
    }

    addReply(c, shared.cone);
    keyModified(c, c->db, c->argv[1], new, 1);
    notifyKeyspaceEvent(NOTIFY_STRING, "setmax", c->argv[1], c->db->id);
    server.dirty++;
}
'''

ANCHOR = '''void psetexCommand(client *c) {
    c->argv[3] = tryObjectEncoding(c->argv[3]);
    setGenericCommand(c, OBJ_PX, c->argv[1], &(c->argv[3]), c->argv[2], UNIT_MILLISECONDS, NULL, NULL, NULL);
}
'''

DECL_ANCHOR = 'void setnxCommand(client *c);\n'


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: apply_setmax.py <redis-source-dir> [--no-dirty]")
    root = sys.argv[1]
    keep_dirty = '--no-dirty' not in sys.argv
    here = os.path.dirname(os.path.abspath(__file__))

    shutil.copyfile(os.path.join(here, 'setmax.json'),
                    os.path.join(root, 'src', 'commands', 'setmax.json'))

    frag = FRAGMENT if keep_dirty else FRAGMENT.replace('    server.dirty++;\n', '')
    path = os.path.join(root, 'src', 't_string.c')
    text = open(path, encoding='utf-8').read()
    assert text.count(ANCHOR) == 1, 'anchor not found in t_string.c'
    text = text.replace(ANCHOR, ANCHOR + frag, 1)
    open(path, 'w', encoding='utf-8').write(text)

    path = os.path.join(root, 'src', 'server.h')
    text = open(path, encoding='utf-8').read()
    assert text.count(DECL_ANCHOR) == 1, 'anchor not found in server.h'
    text = text.replace(DECL_ANCHOR,
                        DECL_ANCHOR + 'void setmaxCommand(client *c);\n', 1)
    open(path, 'w', encoding='utf-8').write(text)
    print('patched', root, 'dirty-counting' if keep_dirty else 'WITHOUT server.dirty++')


main()
