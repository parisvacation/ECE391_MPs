#include "skyline.h"
#include "console.h"
#include "string.h"
// Mock frame buffer for testing
uint16_t test_frame_buffer[SKYLINE_WIDTH * SKYLINE_HEIGHT];

// External variables defined in skyline.h
struct skyline_star skyline_stars[SKYLINE_STARS_MAX];
uint16_t skyline_star_cnt;
struct skyline_window* skyline_win_list;
struct skyline_beacon skyline_beacon;


void test_add_rm_star(){
    skyline_star_cnt = 0;
    add_star(10, 10, 0x1234);
    add_star(20, 20, 0x5678);
    add_star(30, 30, 0x9abc);
    add_star(40, 40, 0xdef0);
    console_printf("Star count: %d\n", skyline_star_cnt);
    console_printf("Star 0: (%d, %d, 0x%04x)\n", skyline_stars[0].x, skyline_stars[0].y, skyline_stars[0].color);
    console_printf("Star 1: (%d, %d, 0x%04x)\n", skyline_stars[1].x, skyline_stars[1].y, skyline_stars[1].color);
    console_printf("Star 2: (%d, %d, 0x%04x)\n", skyline_stars[2].x, skyline_stars[2].y, skyline_stars[2].color);
    console_printf("Star 3: (%d, %d, 0x%04x)\n", skyline_stars[3].x, skyline_stars[3].y, skyline_stars[3].color);
    remove_star(20, 20);
    console_printf("Star count: %d\n", skyline_star_cnt);
    console_printf("Star 0: (%d, %d, 0x%04x)\n", skyline_stars[0].x, skyline_stars[0].y, skyline_stars[0].color);
    console_printf("Star 1: (%d, %d, 0x%04x)\n", skyline_stars[1].x, skyline_stars[1].y, skyline_stars[1].color);
    console_printf("Star 2: (%d, %d, 0x%04x)\n", skyline_stars[2].x, skyline_stars[2].y, skyline_stars[2].color);
    remove_star(10, 10);
    console_printf("Star count: %d\n", skyline_star_cnt);
    console_printf("Star 0: (%d, %d, 0x%04x)\n", skyline_stars[0].x, skyline_stars[0].y, skyline_stars[0].color);
    console_printf("Star 1: (%d, %d, 0x%04x)\n", skyline_stars[1].x, skyline_stars[1].y, skyline_stars[1].color);
    remove_star(50, 50);
    console_printf("Star count: %d\n", skyline_star_cnt);
    console_printf("Star 0: (%d, %d, 0x%04x)\n", skyline_stars[0].x, skyline_stars[0].y, skyline_stars[0].color);
    console_printf("Star 1: (%d, %d, 0x%04x)\n", skyline_stars[1].x, skyline_stars[1].y, skyline_stars[1].color);
    remove_star(40, 40);
    console_printf("Star count: %d\n", skyline_star_cnt);
    console_printf("Star 0: (%d, %d, 0x%04x)\n", skyline_stars[0].x, skyline_stars[0].y, skyline_stars[0].color);
    remove_star(30, 30);
    console_printf("Star count: %d\n", skyline_star_cnt);

    console_printf("\n");

    //Test the maximum of the star array
    for (int i = 1; i <= 1001; i++){
        add_star(i, i, 0x1234);
        if (i % 100 == 1){
            console_printf("Star count: %d\n", skyline_star_cnt);
        }
    }
    for (int i = 1; i <= 1001; i++){
        remove_star(i, i);
        if (i % 100 == 1){
            console_printf("Star count: %d\n", skyline_star_cnt);
        }
    }
}

void test_draw_star() {
    console_printf("\n--- Testing draw_star ---\n");
    
    // Clear frame buffer
    memset(test_frame_buffer, 0, sizeof(test_frame_buffer));
    skyline_star_cnt = 0;
    
    // Draw a star
    struct skyline_star test_star = {100, 100, 1, 0xffff};
    struct skyline_star test_star2 = {98, 98, 1, 0xffff};
    struct skyline_star test_star3 = {102, 102, 1, 0xffff};
    add_star(100, 98, 0xffff);
    add_star(102, 98, 0xffff);

    draw_star(test_frame_buffer, &test_star);
    draw_star(test_frame_buffer, &test_star2);
    draw_star(test_frame_buffer, &test_star3);
    draw_star(test_frame_buffer, &skyline_stars[0]);
    draw_star(test_frame_buffer, &skyline_stars[1]);
    // Check if star is drawn correctly
    int star_pixels = 0;
    for (int y = 98; y < 103; y++) {
        for (int x = 98; x < 103; x++) {
            if (test_frame_buffer[y * 640 + x] == 0xffff) {
                star_pixels++;
            }
        }
    }
    console_printf("Number of pixels drawn for star: %d\n", star_pixels);
    if (star_pixels == 5) console_printf("Test Pass.\n");
    else console_printf("Test fail!\n");
}

int count_windows() {
    int count = 0;
    struct skyline_window *current = skyline_win_list;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

struct skyline_window* find_window(uint16_t x, uint16_t y, uint8_t w, uint8_t h, uint16_t color) {
    struct skyline_window *current = skyline_win_list;
    while (current != NULL) {
        if (current->x == x && current->y == y && current->w == w && 
            current->h == h && current->color == color) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void test_add_rm_window() {
    console_printf("\n--- Testing add_window ---\n");

    // Initialize skyline_windows to an empty list
    skyline_win_list = NULL;

    // Simple test case
    console_printf("Adding simple window...\n");
    add_window(10, 10, 5, 5, 0x1111);
    console_printf("Simple window added. Count: %d\n", count_windows());

    // Test case 1: Add a single window
    console_printf("Adding window 1...\n");
    add_window(100, 100, 50, 50, 0xFFFF);
    console_printf("Window 1 added. Count: %d\n", count_windows());

    if (count_windows() == 2 && find_window(100, 100, 50, 50, 0xFFFF) != NULL) {
        console_printf("Test case 1: PASSED\n");
    } else {
        console_printf("Test case 1: FAILED!!\n");
    }
    console_printf("\n");

    // Test case 2: Add multiple windows
    console_printf("Adding window 2...\n");
    add_window(200, 200, 30, 40, 0xAAAA);
    console_printf("Window 2 added. Count: %d\n", count_windows());

    console_printf("Adding window 3...\n");
    add_window(300, 300, 20, 25, 0x5555);
    console_printf("Window 3 added. Count: %d\n", count_windows());

    if (count_windows() == 4 && 
        find_window(200, 200, 30, 40, 0xAAAA) != NULL &&
        find_window(300, 300, 20, 25, 0x5555) != NULL) {
        console_printf("Test case 2: PASSED\n");
    } else {
        console_printf("Test case 2: FAILED!!\n");
    }
    // Print current window count
    console_printf("Current windows added: %d\n", count_windows());
    console_printf("\n");

    console_printf("Removing window 1...\n");
    remove_window(100, 100);
    console_printf("Window 1 removed. Count: %d\n", count_windows());

    console_printf("Removing window 3...\n");
    remove_window(300, 300);

    console_printf("Removing unexisting window...\n");
    remove_window(150, 150);
    if(count_windows() == 2 
        && find_window(100, 100, 50, 50, 0xFFFF) == NULL
        && find_window(300, 300, 20, 25, 0x5555) == NULL
    ) console_printf("Test case 3: PASSED\n");
    else console_printf("Test case 3: FAILED!\n");
    // Print final window count
    console_printf("Final windows added: %d\n", count_windows());
}

void test_draw_window(){
    console_printf("\n--- Testing draw_window ---\n");

    // Clear frame buffer
    memset(test_frame_buffer, 0, sizeof(test_frame_buffer));
    skyline_win_list = NULL;

    add_window(450, 450, 10, 12, 0x1234);
    add_window(470, 475, 5, 10, 0x1234);
    add_window(440, 490, 6, 8, 0x1234);

    draw_window(test_frame_buffer, skyline_win_list);
    draw_window(test_frame_buffer, skyline_win_list->next);
    draw_window(test_frame_buffer, (skyline_win_list->next)->next);

    // Check if window is drawn correctly
    int window_pixels = 0;
    for (int y = 430; y < 480; y++) {
        for (int x = 430; x < 480; x++) {
            if (test_frame_buffer[y * 640 + x] == 0x1234) {
                window_pixels++;
            }
        }
    }
    console_printf("Number of pixels drawn for window: %d\n", window_pixels);
    if (window_pixels == 145) console_printf("Test Pass.\n");
    else console_printf("Test fail!\n");
}

void test_draw_beacon(){
    console_printf("\n--- Testing draw_window ---\n");

    // Clear frame buffer
    memset(test_frame_buffer, 0, sizeof(test_frame_buffer));

    //Create img array
    const uint16_t img[8 * 8] = {
        [0 ... 63] = 0xFFFF
    };

    start_beacon(img, 635, 475, 8, 10, 5);

    draw_beacon(test_frame_buffer, 15, &skyline_beacon);
    draw_beacon(test_frame_buffer, 26, &skyline_beacon);
    // Check if beacon is drawn correctly
    int beacon_pixels = 0;
    for (int y = 470; y < 480; y++) {
        for (int x = 620; x < 640; x++) {
            if (test_frame_buffer[y * 640 + x] == 0xffff) {
                beacon_pixels++;
            }
        }
    }
    console_printf("Number of pixels drawn for beacon: %d\n", beacon_pixels);
    if (beacon_pixels == 0) console_printf("Test1 Pass.\n");
    else console_printf("Test1 fail!\n");

    console_printf("\n");
    draw_beacon(test_frame_buffer, 34, &skyline_beacon);
    for (int y = 470; y < 480; y++) {
        for (int x = 620; x < 640; x++) {
            if (test_frame_buffer[y * 640 + x] == 0xffff) {
                beacon_pixels++;
            }
        }
    }
    console_printf("Number of pixels drawn for beacon: %d\n", beacon_pixels);
    if (beacon_pixels == 25) console_printf("Test2 Pass.\n");
    else console_printf("Test2 fail!\n");
}

int main(){
    test_add_rm_star();
    test_draw_star();
    test_add_rm_window();
    test_draw_window();
    test_draw_beacon();
    
    return 0;
}