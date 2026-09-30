## Zone-based Performance Profiler

The profiler measures blocks of code, or zones, which the programmer defines with the START_BLOCK("name") and END_BLOCK() macros.
For each block, the profiler will report how many times this code block ran, it's total running time, the number of branch misses 
and the percent of branches that were not predicted, the number of L1 data cache misses and the percent of misses that were caused
by loads, and the number of L1 data Translation Look-aside Buffer misses. Use the PRINT_PROFILER macro to print the report to stdout.
Use the RESET_PROFILER macro when you want to reset the profiler, for instance before starting a new frame.

Here's a simple example of a program that reads in a json file, parses the json, and performs some computation on it.

```int main(int argc, char *argv[]) {
  SETUP_PROFILER;
  char *filename = argv[1];

  START_BLOCK("read json");
  char *jsonfile = read_json(filename);
  END_BLOCK;

  START_BLOCK("parse json");
  JSON json = parse_json(jsonfile);
  END_BLOCK;

  START_BLOCK("computation");
  computation(json);
  END_BLOCK;

  PRINT_PROFILER;
  return 0;
}```

Given a 1GB input json file, this example program outputs the following performance report:

```+------------------------------------------------------------------------------+
|               name id  hits    time     branch miss      L1d miss    L1d TLB |
|------------------------------------------------------------------------------|
|          read json  1     1  204.5ms  571.0  ( 0.0%)  328.0  (27.7%)  142.0  |
|         parse json  2     1   11.0s    19.9g (21.8%)  165.5m (82.5%)   11.4m |
|        computation  3     1  751.8ms  490.1m (11.5%)   19.7m (77.1%)  594.8k |
+------------------------------------------------------------------------------+```

