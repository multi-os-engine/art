AHAT - Android Heap Analysis Tool

Usage:
  java -jar ahat.jar [-p port] FILE
    Launch an http server for viewing the given Android heap-dump FILE.

  Options:
    -p <port>
       Serve pages on the given port. Defaults to 7100.

TODO:
 * /objects query fails with no parameters.
   And probably the same for many of the handlers.
 * Add more tips to the help page.
   - Note that only 'app' heap matters, not 'zygote' or 'image'.
   - Say what a dex cache is.
   - Recommend how to start looking at a heap dump.
   - Say how to enable allocation sites.
   - Where to submit feedback, questions, and bug reports.
 * Submit perflib fix for getting stack traces, then uncomment that code in
   AhatSnapshot to use that.
 * Dim 'image' and 'zygote' heap sizes slightly? Why do we even show these?
 * Filter out RootObjs in mSnapshot.getGCRoots, not RootsHandler.
 * Let user re-sort sites objects info by clicking column headers.
 * Let user re-sort "Objects" list.
 * Show site context and heap and class filter in "Objects" view?
 * What should the 'Object Info' in the object view show and how?
   - It looks a little silly now.
 * Have a menu at the top of an object view with links to the sections?
 * Include ahat version and hprof file in the menu at the top of the page?
 * Heaped Table
   - Make sortable by clicking on headers.
   - Use consistent order for heap columns.
      Sometimes I see "app" first, sometimes last (from one heap dump to
      another) How about, always sort by name?
 * For long strings, limit the string length shown in the summary view to
   something reasonable.  Say 50 chars, then add a "..." at the end.
 * For string summaries, if the string is an offset into a bigger byte array,
   make sure to show just the part that's in the bigger byte array, not the
   entire byte array.
 * Style sheets don't work on elinks? Is that true?
 * For HeapTable with single heap shown, the heap name isn't centered?
 * Don't use FieldReaderException. Have a non-exception way to report match
   failure. This is showing up in profiling as taking a long time, and doesn't
   seem to like a multithreaded executor.
 * Set up an infrastructure for writing test cases.

 * [low priority] by site allocations won't line up if the stack has been
   truncated. Is there any way to manually line them up in that case?

 * [low priority] Have a switch to choose whether unreachable objects are
   ignored or not?  Is there any interest in what's unreachable, or is it only
   reachable objects that people care about?

 * [low priority] Have a way to diff two heap dumps by site.
   This should be pretty easy to do, actually. The interface is the real
   question. Maybe: augment each byte count field on every page with the diff
   if a baseline has been provided, and allow the user to sort by the diff.

Things to Test:
 * That we can open a hprof without an 'app' heap and show a tabulation of
   objects normally sorted by 'app' heap by default.
 * Visit /objects without parameters and verify it doesn't throw an exception.

Perflib Requests:
 * Class objects should have java.lang.Class as their class object, not null.
 * ArrayInstance should have asString() to get the string, without requiring a
   length function.
 * Document that getHeapIndex returns -1 for no such heap.
 * Look up totalRetainedSize for a heap by Heap object, not by a separate heap
   index.
 * What's the difference between getId and getUniqueId?
 * I see objects with duplicate references.
 * Don't store stack trace by heap (CL 157252)
 * A way to get overall retained size by heap.
 * A method Instance.isReachable()

Things to move to perflib:
 * Extracting the string from a String Instance.
 * Extracting bitmap data from bitmap instances.
 * Adding up allocations by stack frame.
 * Computing, for each instance, the other instances it dominates.

