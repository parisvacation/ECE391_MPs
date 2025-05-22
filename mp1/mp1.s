# Name: Libin Wang
# NetID: libin2
# Writing time: 12/09/2024 
# mp1.s - Your solution goes here
#
        .equ SKYLINE_STARS_MAX, 1000
        .equ SKYLINE_WIDTH, 640
        .equ SKYLINE_HEIGHT, 480
        .equ PIXEL_MEMORY, 2
        .equ STAR_MEMORY, 8

        .section .data                    # Data section (optional for global data)
        .extern skyline_beacon            # Declare the external variables
        .extern skyline_stars
        .extern skyline_win_list

        .global skyline_star_cnt          # Declare the global variable
        .type   skyline_star_cnt, @object


        .text                             # Text(Code) section, extern functions
        .global add_star
        .type   add_star, @function

        .global remove_star
        .type   remove_star, @function

        .global draw_star
        .type   draw_star, @function

        .global add_window
        .type   add_window, @function
  
        .global remove_window
        .type   remove_window, @function
 
        .global draw_window
        .type   draw_window, @function
     
        .global start_beacon
        .type   start_beacon, @function
        
        .global draw_beacon
        .type   draw_beacon, @function


# Add a star at position (x,y) to the skyline stars array with the specified color,
# and update "skyline_star_cnt".
# extern void add_star(uint16_t x, uint16_t y, uint16_t color);      
add_star: #a0 -- x; a1 -- y; a2 -- color
        addi sp, sp, -32                 # Allocate stack space
        sd ra, 24(sp)                    # Save return address
        sd fp, 16(sp)                    # Save previous frame pointer
        sd s1, 8(sp)                     # Save Callee-save register

        li t1, SKYLINE_STARS_MAX         # Check if there is left room
        la t0, skyline_star_cnt
        lhu s1, 0(t0)                    # Register s1 saves the number of stars
        beq s1, t1, add_star_done

        la t0, skyline_stars             # Find the location to insert star
        li t2, STAR_MEMORY               # The struct 'skyline_star' is 8-byte aligned
        mul t3, s1, t2
        add t0, t0, t3

        sh a0, 0(t0)                     # Load x into array
        sh a1, 2(t0)                     # Load y into array
        li t4, 1                         # Load diameter(default 1)
        sb t4, 4(t0)
        sh a2, 6(t0)                     # Load color into array

        addi s1, s1, 1                   # Update the star count
        la t0, skyline_star_cnt
        sh s1, 0(t0)                 

add_star_done:  
        ld s1, 8(sp)                     # Restore Callee-save register                           
        ld fp, 16(sp)                    # Restore previous frame pointer   
        ld ra, 24(sp)                    # Restore return address
        addi sp, sp, 32                  # Deallocate stack space
        ret                              # Return to caller


# Remove the star at position (x,y) from the skyline stars array.
# If exists, it should update the star count.
# extern void remove_star(uint16_t x, uint16_t y);
remove_star: # a0 -- x; a1 -- y
        addi sp, sp, -32                 # Allocate stack space
        sd ra, 24(sp)                    # Save
        sd fp, 16(sp)   
        sd s1, 8(sp)                  

        la t0, skyline_star_cnt          # Find whether there is star at position(x, y)
        lhu s1, 0(t0)                    # Register s1 behave as counter of stars
        li t1, 0
        la t0, skyline_stars             # Load the starting point of star array

find_star:
        beq s1, t1, remove_star_done
        lhu t2, 0(t0)                    # Check if the x and y coordinate are equal
        lhu t3, 2(t0)                    
        xor t2, a0, t2
        xor t3, a1, t3
        or t4, t2, t3
        beqz t4, remove_progress

        addi s1, s1, -1                  # Load the next point of star array
        addi t0, t0, STAR_MEMORY         # The struct 'skyline_star' is 8-byte aligned
        j find_star

remove_progress:
        li t5, 1                         # Check if the removed star is the last one in array
        beq s1, t5, remove_last_star

        addi s1, s1, -1                  # Calculate the location of the final star
        li t6, STAR_MEMORY
        mul t5, s1, t6
        mv t1, t0                        # Copy the address of removed star to Register t1
        add t0, t0, t5

        lhu t5, 0(t0)                    # Copy the elements of last star into removed location,
        sh t5, 0(t1)                     # so that we can remove the target star;
        lhu t5, 2(t0)
        sh t5, 2(t1)
        lbu t5, 4(t0)
        sb t5, 4(t1)
        lhu t5, 6(t0)
        sh t5, 6(t1)                     # After this, we should remove the last star in the array now

remove_last_star:
        li t5, 0
        sh t5, 0(t0)
        sh t5, 2(t0)
        sb t5, 4(t0)
        sh t5, 6(t0)

decrease_star_count:
        la t0, skyline_star_cnt          # Update the star count
        lhu s1, 0(t0)
        addi s1, s1, -1
        sh s1, 0(t0)

remove_star_done:
        ld s1, 8(sp)                     # Restore 
        ld fp, 16(sp)
        ld ra, 24(sp)
        addi sp, sp, 32                  # Deallocate stack space
        ret


# Draw a star to the frame buffer
# extern void draw_star(uint16_t * fbuf, const struct skyline_star * star);
draw_star: # a0 -- fbuf; a1 -- star
        addi sp, sp, -32                 # Allocate stack space         
        sd ra, 24(sp)                    # Save
        sd fp, 16(sp)
        sd s1, 8(sp)

        lhu t1, 0(a1)                    # Load x and y coordinate into Registers t1 and t2
        lhu t2, 2(a1)
        lhu t3, 6(a1)                    # Load color of star in Register t3 

        li t4, SKYLINE_WIDTH 
        li s1, PIXEL_MEMORY              # Calculate the memory offset

        mul t2, t2, t4                   # y_coordinate * 640
        add t1, t1, t2                   # y_coordinate * 640 + x_coordinate
        mul t1, t1, s1                   # (y_coordinate * 640 + x_coordinate) * 2
        add t1, a0, t1                   # Store the top-left pixel of the star in Register t1

        sh t3, 0(t1)                     # Draw the star

draw_star_done:
        ld s1, 8(sp)                     # Restore                     
        ld fp, 16(sp)
        ld ra, 24(sp)
        addi sp, sp, 32                  # Deallocate stack space
        ret


# Add a window of the specified color and size at the specified position to the window list
# extern void add_window (uint16_t x, uint16_t y, uint8_t w, uint8_t h, uint16_t color);
add_window: # a0 -- x; a1 -- y; a2 -- w; a3 -- h; a4 -- color 
        addi sp, sp, -16                 # Allocate stack space         
        sd ra, 8(sp)                     # Save
        sd fp, 0(sp)

        addi sp, sp, -48                 # Caller-save reigsters
        sd a0, 40(sp)
        sd a1, 32(sp)
        sd a2, 24(sp)
        sd a3, 16(sp)
        sd a4, 8(sp)

        addi a0, x0, 16                  # Allocate memory for new node
        call malloc
        mv t0, a0                        # Move Register a0 to t0

        ld a0, 40(sp)                    # Restore registers
        ld a1, 32(sp)
        ld a2, 24(sp)
        ld a3, 16(sp)
        ld a4, 8(sp)
        addi sp, sp, 48

        beqz t0, add_window_done         # Check if allocation fails

        li t1, 0                         # Store values in the 'window'
        sd t1, 0(t0)                     # Set NULL
        sh a0, 8(t0)                      
        sh a1, 10(t0)
        sb a2, 12(t0)
        sb a3, 13(t0)
        sh a4, 14(t0)

        la t1, skyline_win_list          # Find the head pointer
        ld t2, 0(t1)        
        sd t2, 0(t0)                     # new_node->next = head
        sd t0, 0(t1)                     # head = new_node          

add_window_done:                     
        ld fp, 0(sp)                     # Restore                     
        ld ra, 8(sp)
        addi sp, sp, 16                  # Deallocate stack space
        ret


# Remove a window the upper left corner of which is at position (x,y), if exists
# extern void remove_window(uint16_t x, uint16_t y);
remove_window: # a0 -- x; a1 -- y
        addi sp, sp, -16                 # Allocate stack space
        sd ra, 8(sp)                    # Save
        sd fp, 0(sp)

        la t0, skyline_win_list
        ld t1, 0(t0)
        li t5, 0                         # Register t5 saves the pre-pointer of t1

loop_linked_list:
        beqz t1, remove_window_done

        lhu t2, 8(t1)                    # Check if x and y coordinates are the same
        lhu t3, 10(t1)
        xor t2, a0, t2
        xor t3, a1, t3
        or t2, t2, t3
        beqz t2, remove_window_progress

        ld t4, 0(t1)                     # Get the next node in the list
        mv t5, t1
        mv t1, t4
        j loop_linked_list

remove_window_progress:
        beqz t5, remove_first_window     # Check if the window to remove is the first one
        ld t4, 0(t1)                     # Remove the 'window'
        sd t4, 0(t5)
        j free_memory

remove_first_window:
        ld t4, 0(t1)                     # head = head->next
        la t0, skyline_win_list
        sd t4, 0(t0)

free_memory:
        mv a0, t1                        # Deallocate memory space
        call free   
        li a0, 0                         # Set allocated pointer to null 
        li t1, 0   
        
remove_window_done:
        ld fp, 0(sp)                     # Restore
        ld ra, 8(sp)
        addi sp, sp, 16                  # Deallocate stack space
        ret


# Draw the given window onto the given frame buffer
# extern void draw_window(uint16_t * fbuf, const struct skyline_window * win);
draw_window: # a0 -- fbuf; a1 -- win
        addi sp, sp, -48                 # Allocate stack space
        sd ra, 40(sp)                    # Save
        sd fp, 32(sp)
        sd s1, 24(sp)
        sd s2, 16(sp)
        sd s3, 8(sp)

        li s1, SKYLINE_WIDTH             # Save the width and height of frame buffe in Registers s1 and s2
        li s2, SKYLINE_HEIGHT

        li t3, PIXEL_MEMORY    
        lhu t1, 8(a1)                    # Check if x or y coordinate is out of space                   
        lhu t2, 10(a1)
        bgeu t1, s1, draw_window_done
        bgeu t2, s2, draw_window_done

        # Calculate the memory offset, and store the top-left pixel of the window in Register t1
        mul t2, t2, s1                   # y_coordinate * 640
        add t1, t1, t2                   # y_coordinate * 640 + x_coordinate
        mul t1, t1, t3                   # (y_coordinate * 640 + x_coordinate) * 2
        add t1, a0, t1

        lbu t3, 13(a1)                   # Save the height in Register t3 as counter
        mv t4, t3                   
        lhu s3, 14(a1)                   # Load the color of window

draw_window_column:
        beqz t3, draw_window_done        # Check the column counter

        sub t5, t4, t3                   # Check if y coordinate is out of space
        lhu t6, 10(a1)                   # Sace y coordinate in Register t6
        add t6, t6, t5
        bgeu t6, s2, draw_window_done  

        mul t5, t5, s1                   # Find the starting pixel of current row (in t5)
        li t6, PIXEL_MEMORY
        mul t5, t5, t6
        add t5, t1, t5

        addi t3, t3, -1                  # Count down the counter
        lbu t2, 12(a1)                   # Save the width in Register t2 as counter
        lhu t6, 8(a1)                    # Save x coordinate in Register t6
        beqz t2, draw_window_column      # If the width of 'window' is zero, turn to next column

draw_window_row:
        sh s3, 0(t5)                     # Color the pixel

        addi t6, t6, 1                   # Check if x coordinate is out of space
        bgeu t6, s1, draw_window_column

        addi t5, t5, PIXEL_MEMORY
        addi t2, t2, -1                  # Count down the counter
        beqz t2, draw_window_column      # Check the row counter
        j draw_window_row

draw_window_done:
        ld s3, 8(sp)                     # Restore
        ld s2, 16(sp)                    
        ld s1, 24(sp)
        ld fp, 32(sp)
        ld ra, 40(sp)
        addi sp, sp, 48                  # Deallocate stack space
        ret


# Initialize all fields of the skyline_beacon struct
# extern void start_beacon (const uint16_t * img, uint16_t x, uint16_t y, uint8_t dia, uint16_t period, uint16_t ontime);
start_beacon:

        la t0, skyline_beacon             # Load address of skyline_beacon into t0 (t0 is 64-bit)

        # Store the function arguments into the struct fields
        sd a0, 0(t0)                      # Store img (a0, 64-bit) at offset 0 (8 bytes)

        sh a1, 8(t0)                      # Store x (a1, 16-bit) at offset 8 (after img pointer)

        sh a2, 10(t0)                     # Store y (a2, 16-bit) at offset 10

        sb a3, 12(t0)                     # Store dia (a3, 8-bit) at offset 12

        sh a4, 14(t0)                     # Store period (a4, 16-bit) at offset 14

        sh a5, 16(t0)                     # Store ontime (a5, 16-bit) at offset 16

        ret                               # Return to caller


# Draw the beacon described by the passed skyline beacon struct, only when time is meant to be on
# extern void draw_beacon (uint16_t * fbuf, uint64_t t, const struct skyline_beacon * bcn);
draw_beacon: # a0 -- fbuf; a1 -- t; a2 -- bcn
        addi sp, sp, -32                  # Allocate memory space
        sd ra, 24(sp)                     # Save
        sd fp, 16(sp)
        sd s1, 8(sp)
        sd s2, 0(sp)

        li s1, SKYLINE_WIDTH             # Save the width and height of frame in Registers s1 and s2
        li s2, SKYLINE_HEIGHT

        lhu t1, 14(a2)                    # Load 'period' in Register t1
        lhu t2, 16(a2)                    # Load 'ontime' in Register t2
        remu t1, a1, t1                   
        bgeu t1, t2, draw_beacon_done     # If "t % period" is smaller to "ontime", 
                                          # the beacon should be drawn

        lhu t1, 8(a2)                     # Check if the x or y coordinate is out of space
        lhu t2, 10(a2)
        bgeu t1, s1, draw_beacon_done
        bgeu t2, s2, draw_beacon_done


        # Find the top-left pixel of beacon, and store it in Register t1
        li t3, PIXEL_MEMORY
        mul t2, t2, s1                    # y_coordinate * 640
        add t1, t1, t2                    # y_coordinate * 640 + x_coordinate
        mul t1, t1, t3                    # (y_coordinate * 640 + x_coordinate) * 2
        add t1, a0, t1

        lbu t2, 12(a2)                    # Load the diameter in Register t2 and t3 (as counter)
        mv t3, t2
        
draw_beacon_column:
        beqz t2, draw_beacon_done

        sub t4, t3, t2
        lhu t5, 10(a2)                    # Check if y_coordinate is out of space
        add t5, t5, t4  
        bgeu t5, s2, draw_beacon_done

        mv a3, t4                         # Copy the relative y coordinate in Register a3

        mul t4, t4, s1                    # Find the starting pixel of current row (in t4)
        li t5, PIXEL_MEMORY
        mul t4, t4, t5
        add t4, t1, t4                    

        addi t2, t2, -1                   # Count down the counter
        lbu t5, 12(a2)                    # Let Register t5 become row counter
        li t6, 0                          # Load relative x coordinate in Register t6

draw_beacon_row:
        lbu a5, 12(a2)                    # Calculate the RGB color(16-bit)
        mul a4, a3, a5                    
        add a4, a4, t6                 
        li a5, 2                          # Variable 'img' is a 'uint_16' array
        mul a4, a4, a5                    # Store the memory offset in Register a4

        ld a5, 0(a2)                      # The 'img' pointer(Array)
        add a4, a4, a5                                         
        lhu a5, 0(a4)                     # Load the RGB color in Register a5

        sh a5, 0(t4)                      # Color the pixel
        
        addi t6, t6, 1                    # Draw next pixel  
        lhu a5, 8(a2)                     # Check if x coordinate is out of space
        add a5, a5, t6
        bgeu a5, s1, draw_beacon_column

        addi t4, t4, PIXEL_MEMORY
        addi t5, t5, -1                   # Count down the counter
        beqz t5, draw_beacon_column       # Check the counter
        j draw_beacon_row

draw_beacon_done:
        ld s2, 0(sp)                      # Restore
        ld s1, 8(sp)
        ld fp, 16(sp)
        ld ra, 24(sp)
        addi sp, sp, 32                   # Deallocate memory space
        ret

        .end
