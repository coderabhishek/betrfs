/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// verify that the table lock log entry is handled

#include <sys/stat.h>
#include "test.h"


static const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

static DB_ENV *env;
static DB_TXN *tid;
static DB     *db;
static DBT key,data;
static int i;
enum {N=10000};
//static char *keys[N];
static char ** keys;
//static char *vals[N];
static char ** vals;

static void gracefully_shutdown(DB_TXN * oldest) {
    oldest->commit(oldest, 0);
    env->close(env, 0);
    printf("shutdown\n");
    //toku_hard_crash_on_purpose();
} 

static void
do_x1_shutdown (void) {
    DB_TXN *oldest;   
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);                                                     assert(r==0);

    r=db_env_create(&env, 0);                                                  assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_THREAD, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
   {

        r=env->txn_begin(env, 0, &oldest, 0);
        CKERR(r);
    }

    r=db_create(&db, env, 0);                                                  CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    r=tid->commit(tid, 0);                                                     assert(r==0);

    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    for (i=0; i<N; i++) {
	r=db->put(db, tid, dbt_init(&key, keys[i], strlen(keys[i])+1), dbt_init(&data, vals[i], strlen(vals[i])+1), 0);    assert(r==0);
	if (i%500==499) {
	    r=tid->commit(tid, 0);                                                     assert(r==0);
	    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
	}
    }
    r=tid->commit(tid, 0);                                                     assert(r==0);

    r=db->close(db, 0);                                                        assert(r==0);
    gracefully_shutdown(oldest);
}

static void
do_x1_recover (bool UU(did_commit)) {
    int r;
   // char glob[TOKU_PATH_MAX+1];
    char * glob = (char *) toku_xmalloc(sizeof(char) * (TOKU_PATH_MAX +1));
    toku_os_recursive_delete(toku_path_join(glob, 2, TOKU_TEST_FILENAME, "*.tokudb"));

    r=db_env_create(&env, 0);                                                  assert(r==0);
    env->set_errfile(env, stderr);
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_THREAD|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
    r=db_create(&db, env, 0);                                                  CKERR(r);
    r=db->open(db, tid, "foo.db", 0, DB_BTREE, 0, S_IRWXU+S_IRWXG+S_IRWXO);                       CKERR(r);
    for (i=0; i<N; i++) {
	r=db->get(db, tid, dbt_init(&key, keys[i], 1+strlen(keys[i])), dbt_init_malloc(&data), 0);     assert(r==0);
	assert(strcmp((char*)data.data, vals[i])==0);
	toku_free(data.data);
	data.data=0;
	if (i%500==499) {
	    r=tid->commit(tid, 0);                                                     assert(r==0);
	    r=env->txn_begin(env, 0, &tid, 0);                                         assert(r==0);
	}
    }
    r=tid->commit(tid, 0);                                                     assert(r==0);
    toku_free(data.data);
    r=db->close(db, 0);                                                        CKERR(r);
    r=env->close(env, 0);                                                      CKERR(r);
    toku_free(glob);
}



static void internal_recover_test3(bool do_commit, bool do_recover_committed) {
    srandom(0xDEADBEEF);
    for (i=0; i<N; i++) {
	//char ks[100];  
        char * ks = (char *) toku_xmalloc(sizeof(char) * 100);
        snprintf(ks, 100, "k%09ld.%d", random(), i);
	//char vs[1000]; 
        char * vs = (char *) toku_xmalloc(sizeof(char) * 1000);
        snprintf(vs, 1000, "v%d.%0*d", i, (int)(1000-100), i);
	keys[i]=toku_strdup(ks);
	vals[i]=toku_strdup(vs);
        toku_free(ks);
        toku_free(vs);
    }
    if (do_commit) {
	do_x1_shutdown();
    } else if (do_recover_committed) {
	do_x1_recover(true);
    } 
    for (i=0; i<N; i++) {
        toku_free(keys[i]);
        toku_free(vals[i]);
    }

}
extern "C" int test_recover_test3(void);
int test_recover_test3(void) {
    pre_setup();
    keys = (char **) toku_xmalloc(sizeof(char *) * (N));
    vals = (char **) toku_xmalloc(sizeof(char *) * (N));
    internal_recover_test3(true,false);
    internal_recover_test3(false,true);

    toku_free(keys);
    toku_free(vals);
    post_teardown();
    return 0;
}
