/* 

    test_main.c — Unity Test Runner for ESP-IDF

    This file is the entry point for executing unit tests.
    It runs all tests explicitly and optionally starts an interactive menu.

*/

#include "unity.h"

#include "adc.h"    // Component under test
#include "unity_internals.h"

#include <stdio.h>


// Optional banner printing
static void print_banner(const char* text)
{
    printf("\n#### %s ####\n\n", text);
}

// --- List your test functions here ---
// These should match the names in your test_adc.c or other test files
extern void test_buffer_fills_and_wraps(void);
extern void test_apply_bandpass_iir_behavior(void);
extern void test_blink_detection_increments(void);
extern void test_alpha_dominance(void);

void app_main(void)
{

    /* ---------------------------------------------------------- */
    /* These are the different ways of running registered tests.
     * In practice, only one of them is usually needed.
     *
     * UNITY_BEGIN() and UNITY_END() calls tell Unity to print a summary
     * (number of tests executed/failed/ignored) of tests executed between these calls.
     */

    // print_banner("Executing one test by its name");
    // UNITY_BEGIN();
    // unity_run_test_by_name("Mean of an empty array is zero");
    // UNITY_END();

    // print_banner("Running tests with [mean] tag");
    // UNITY_BEGIN();
    // unity_run_tests_by_tag("[mean]", false);
    // UNITY_END();

    // print_banner("Running tests without [fails] tag");
    // UNITY_BEGIN();
    // unity_run_tests_by_tag("[fails]", true);
    // UNITY_END();

    // print_banner("Running all the registered tests");
    // UNITY_BEGIN();
    // unity_run_all_tests();
    // UNITY_END();

/*  ------------------------------------------------------------- */

    print_banner("Running all registered tests (explicit)");

    // Initialize Unity
    UNITY_BEGIN();

    // --- Explicitly run each test ---
    RUN_TEST(test_buffer_fills_and_wraps);
    RUN_TEST(test_apply_bandpass_iir_behavior);
    RUN_TEST(test_blink_detection_increments);
    RUN_TEST(test_alpha_dominance);

    // Add more tests as you create them:
    // RUN_TEST(test_another_functionality);
    // RUN_TEST(test_edge_cases);

    // Print summary
    UNITY_END();

    // print_banner("Starting interactive Unity menu (optional)");

    /*
        Uncomment this section if you want an interactive menu over UART.
        Note: This blocks execution, so disable task watchdog in sdkconfig.defaults.
    */
    // unity_run_menu();

}


