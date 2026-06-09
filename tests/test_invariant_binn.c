#include <check.h>
#include <stdlib.h>
#include <string.h>

#include "libs/binn/binn.c"

START_TEST(test_binn_inflated_size_no_heap_overflow)
{
    /* Invariant: Parsing crafted binn payloads with inflated size fields
       must not cause out-of-bounds writes or crashes. The library must
       reject malformed data gracefully. */

    /* Payload 1: binn list header claiming huge size but tiny actual data */
    unsigned char payload_huge_size[] = {
        0xE0,                         /* type: list */
        0x7F, 0xFF, 0xFF, 0xFF,       /* size: 0x7FFFFFFF (max int) */
        0x00, 0x00, 0x00, 0x01,       /* count: 1 */
        0xA0, 0x10,                   /* string type, length 16 */
        'A', 'A', 'A', 'A', 0x00     /* only 5 bytes of actual string data */
    };

    /* Payload 2: binn map with item size exceeding container */
    unsigned char payload_item_overflow[] = {
        0xE1,                         /* type: map */
        0x00, 0x00, 0x00, 0x0C,       /* size: 12 (small container) */
        0x00, 0x00, 0x00, 0x01,       /* count: 1 */
        0x00, 0x01,                   /* key id: 1 */
        0xC0, 0xFF, 0xFF, 0xFF, 0xFF  /* blob type with huge claimed length */
    };

    /* Payload 3: valid minimal binn list (should parse OK) */
    binn *valid_list = binn_list();
    binn_list_add_int32(valid_list, 42);
    void *valid_buf = binn_ptr(valid_list);
    int valid_size = binn_size(valid_list);

    /* Test inflated size payload */
    binn *obj1 = binn_open(payload_huge_size);
    /* Must either return NULL or not crash; no heap corruption */
    if (obj1) binn_free(obj1);

    /* Test item overflow payload */
    binn *obj2 = binn_open(payload_item_overflow);
    if (obj2) {
        int val = 0;
        binn_map_get_int32(obj2, 1, &val);
        binn_free(obj2);
    }

    /* Test valid payload still works correctly */
    binn *obj3 = binn_open(valid_buf);
    ck_assert_ptr_nonnull(obj3);
    int result = 0;
    BOOL ret = binn_list_get_int32(obj3, 1, &result);
    ck_assert(ret == TRUE);
    ck_assert_int_eq(result, 42);
    binn_free(obj3);
    binn_free(valid_list);
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_binn_inflated_size_no_heap_overflow);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}