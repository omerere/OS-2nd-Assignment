#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_TEAMS 3
#define RUNNERS_PER_TEAM 5
#define TARGET_SCORE 30

void run_single_race(int c) {
    printf("\n==================================================\n");
    printf("STARTING RELAY RACE SIMULATION (Favoritism = %d%%)\n", c);
    printf("==================================================\n");

    // Initialize/Reset the shared scores inside the kernel
    race_init();

    // Create a single Israeli lock (the baton) with coefficient 'c'
    int lock_id = israeli_create(c);
    if (lock_id < 0) {
        printf("Error: Failed to create Israeli lock\n");
        exit(1);
    }

    // Create multiple child processes using fork() and divide into teams
    int total_runners = NUM_TEAMS * RUNNERS_PER_TEAM;
    
    for (int team_id = 0; team_id < NUM_TEAMS; team_id++) {
        for (int runner = 0; runner < RUNNERS_PER_TEAM; runner++) {
            int pid = fork();
            
            if (pid == 0) { // Child process (Runner)
                // Initialize the PRNG using the runner's unique pid
                lcg_srand(getpid());
                
                // Assign team identifier via group id
                setgid(team_id); 
                int my_pid = getpid();

                // Runner Loop
                while (1) {
                    // Acquire the Israeli lock
                    israeli_acquire(lock_id);

                    // Check if any team already crossed the finish line before taking action
                    if (race_get_max() >= TARGET_SCORE) {
                        israeli_release(lock_id);
                        break; 
                    }

                    // Increase the score of its team by 1
                    int current_score = race_inc(team_id);

                    // Print a short message mirroring the requested layout exactly
                    printf("Runner %d (Team %d) acquired the baton\n", my_pid, team_id);
                    printf("Team %d score = %d\n", team_id, current_score);

                    // Track if this particular iteration secured the absolute win
                    if (current_score == TARGET_SCORE) {
                        printf("\n>>> Team %d crossed the finish line first! <<<\n", team_id);
                    }

                    // Release the lock
                    israeli_release(lock_id);

                    // Sleep briefly 
                    sleep(2); 
                }
                exit(0); // Child terminates gracefully
            }
        }
    }

    // Parent process waits for all child runner processes to exit
    for (int i = 0; i < total_runners; i++) {
        wait(0);
    }

    //  record final scores and the winner 
    printf("\n--- FINAL TOURNAMENT RESULTS (Favoritism %d%%) ---\n", c);
    int winning_team = -1; 
    int max_score = -1;
    
    for(int i = 0; i < NUM_TEAMS; i++) {
        int final_score = race_get_score(i);
        printf("Team %d total points: %d\n", i, final_score);
        if(final_score > max_score) {
            max_score = final_score;
            winning_team = i;
        }
    }
    printf("Official Winner: Team %d\n", winning_team);
    printf("--------------------------------------------------\n");

    // Clean up the lock resource
    israeli_destroy(lock_id);
}

int main(int argc, char *argv[]) {
    // Automatically runs all three experimental baselines 
    lcg_srand(getpid());
    int configurations[] = {0, 50, 100};

    for(int i = 0; i < 3; i++) {
        run_single_race(configurations[i]);
    }

    printf("\nAll tournament tracks processed successfully.\n");
    exit(0);
}