#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <rscfl/user/res_api.h>

#define ADVANCED_QUERY_PRINT(...)                                              \
  result = rscfl_advanced_query_with_function(rhdl, __VA_ARGS__);              \
  if (result != NULL) {                                                        \
    printf("timestamp: %llu\nvalue: %f\nsubsystem_name: %s\n",                 \
           result->timestamp, result->value, result->subsystem_name);          \
  } else {                                                                     \
    printf("result was null\n");                                               \
  }

void fn(rscfl_handle rhdl, void* p)
{
  rscfl_token *tkn = p;
  if (tkn != NULL){
    rscfl_free_token(rhdl, tkn);
  }
}

int main(int argc, char** argv) {
  rscfl_handle rhdl;
  printf("hi\n");
  rhdl = rscfl_init("time", 1);

  if(rhdl == NULL) {
    fprintf(stderr,
      "Unable to talk to rscfl kernel module.\n"
      "Check that:\n"
      "\t - rscfl is loaded\n"
      "\t - you have R permissions on /dev/rscfl-data\n"
      "\t - you have RW permissions on /dev/rscfl-ctrl\n");
    return 1;
  }

  /*
   * Storing data into database
   */
    /*token_index = (3*i) % 30;
     *if (rscfl_get_token(rhdl, &(tkns[token_index]))) {
     *  fprintf(stderr, "Failed to get new token\n");
     *  break;
     *}*/
  int err, i, token_index = 0;
  struct accounting acct;
  rscfl_token *tkns[30] = {0};

  for(i = 0; i < 30; i++) {
    if (rscfl_get_token(rhdl, &(tkns[i]))) {
      fprintf(stderr, "Failed to get new token\n");
      break;
    }
  }
  err = rscfl_acct(rhdl, tkns[0], ACCT_START);
  if (err)
    fprintf(stderr, "Error accounting for system call 1 [interest], loop %d\n", i);

  for (i = 0; i < 100; i++){
    if (!(i % 10)){
      printf("Iteration %d\n", i);
    }
    // fopen
    unsigned long long start = get_timestamp();
    FILE* fp = fopen("rscfl_file", "w");
    unsigned long long end = get_timestamp();
    printf("fopen time %llu us\n", end - start);
    /*err = rscfl_acct(rhdl, tkns[token_index], TK_STOP_FL);
     *if (err)
     *  fprintf(stderr, "Error stopping accounting for system call 2 [interest], loop %d\n", i);
     *rscfl_read_and_store_data(rhdl, "{\"function\":\"fopen\"}", tkns[token_index], &fn, tkns[token_index]);*/

    // getuid
    start = get_timestamp();
    getuid();
    end = get_timestamp();
    printf("getuid time %llu us\n", end - start);

    // fclose
    start = get_timestamp();
    fclose(fp);
    end = get_timestamp();
    printf("fclose time %llu us\n", end - start);
    rscfl_switch_token(rhdl, tkns[(i+1)%30]);
  }
  err = rscfl_acct(rhdl, NULL, ACCT_STOP);
  if (err)
    fprintf(stderr, "Error accounting for system call 1 [interest], loop %d\n", i);

  for(i = 0; i < 30; i++) {
    char str[20];
    sprintf(str, "{\"measurement\":\"mod_%d\"}", i);
    rscfl_read_and_store_data(rhdl, str, tkns[i], &fn, tkns[i]);
  }

  /*
   * Querying database
   */
  // sleep(2); // sleep on main thread to give other threads the chance to do their work
  // char *string1 = rscfl_query_measurements(rhdl, "select * from \"cpu.cycles\"");
  // if (string1){
  //   printf("InfluxDB:\nselect * from \"cpu.cycles\"\n%s\n", string1);
  //   free(string1);
  // }
  // mongoc_cursor_t *cursor = query_extra_data(rhdl, "{}", NULL);
  // if (cursor != NULL){
  //   char *string2;
  //   printf("MongoDB:\n");
  //   while (rscfl_get_next_json(cursor, &string2)){
  //     printf("Document from MongoDB:\n%s\n", string2);
  //     rscfl_free_json(string2);
  //   }
  //   mongoc_cursor_destroy(cursor);
  // }

  /*
   * Advanced queries
   */
  // query_result_t *result;
  // ADVANCED_QUERY_PRINT("cpu.cycles", COUNT, NULL)
  // ADVANCED_QUERY_PRINT("cpu.cycles", COUNT, NULL, 1517262516171014, 0)
  // ADVANCED_QUERY_PRINT("cpu.cycles", COUNT, NULL, 1517262516171014)
  // ADVANCED_QUERY_PRINT("cpu.cycles", COUNT, NULL, 0, 1517262516171014)
  // ADVANCED_QUERY_PRINT("cpu.cycles", COUNT, "Filesystem")
  // ADVANCED_QUERY_PRINT("cpu.cycles", COUNT, "Filesystem", 1517262516171014, 0)
  // ADVANCED_QUERY_PRINT("cpu.cycles", COUNT, "Filesystem", 1517262516171014)
  // ADVANCED_QUERY_PRINT("cpu.cycles", COUNT, "Filesystem", 0, 1517262516171014)
  // ADVANCED_QUERY_PRINT("cpu.cycles", MAX, NULL, 1517262516171014, 0)
  // ADVANCED_QUERY_PRINT("cpu.cycles", MAX, "Filesystem", 1517262516171014, 0)
  // ADVANCED_QUERY_PRINT("cpu.cycles", MAX, NULL, 0, 1517262516171014)
  // ADVANCED_QUERY_PRINT("cpu.cycles", MAX, "Filesystem", 0, 1517262516171014)

  // char *result_no_function = rscfl_advanced_query(rhdl, "cpu.cycles", NULL);
  // if (result_no_function != NULL){
  //   printf("%s\n", result_no_function);
  //   free(result_no_function);
  // } else {
  //   fprintf(stderr, "result from advanced query is null\n");
  // }

  // char *extra_data = rscfl_get_extra_data(rhdl, 1517262516171014);
  // printf("data:\n%s\n", extra_data);
  // rscfl_free_json(extra_data);
  // sleep(2);
  // char* result = rscfl_advanced_query(rhdl, "cpu.cycles", NULL,
  //                                     "{\"extra_data\":\"no\"}", 5);
  // if (result) printf("\nresult:%s\n", result);

  rscfl_persistent_storage_cleanup(rhdl);
  return 0;
}
