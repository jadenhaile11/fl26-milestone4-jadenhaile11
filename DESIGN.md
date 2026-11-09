# M4 Decision Record

See specification Section 8.

1. Threads and Ownership

`MainWindow` and the GUI widgets are kept on the main Qt thread so the application stays responsive. When `build_corpus()` runs, a separate thread builds the corpus using a copy of the Workspace. A `std::future` returns the result, and a `QTimer` checks when it finishes. `IpcServer` also uses its own thread for socket communication. Both threads are joined during cleanup.

Constraint 5: The shared `stopping_` variable is written by the main thread and read by the IPC thread. It uses `std::atomic<bool>` to keep this safe. The socket information is set up before the listener starts and cleaned up after it finishes. Build results are passed safely through a promise and future. Workspace data, revisions, and widgets stay on the main thread, while the worker uses its own copy.

2. Build Validity
I used `workspace_revision_` and `build_revision_` to make sure a finished build is still valid. If the workspace changes while a build is running, the numbers will no longer match. Also, when `check_build_completion()` notices this, it discards the outdated result instead of using it.

3. Changes Since the Checkpoint
My original design stayed mostly the same, but I added a few things while actually building the application. I implemented the background worker, `QTimer` checking, and workspace revision tracking as planned. I also added error handling so failed builds are reported instead of leaving the application stuck, along with proper thread cleanup when exiting.

Another change was adding `--build-delay-ms`, which let it simulate a slower build and test whether the GUI stays responsive. Also, the student tests check stale builds, reset behavior, and build failures. Finally, I implemented CR-1 using a separate socket thread so documents can be added remotely without interrupting the GUI. Overall, these changes helped make my original design more reliable and easier to test.

4. CR-1 Impact
For CR-1, I added the IpcServer class with `start()`, `serve()`, and `stop()` to handle communication through a Unix socket. Also created the `aiws_send` program to send document requests and updated `main.cpp` to support the `--ipc-socket` option.

When a request comes in, the server checks whether the file is readable and then passes the request to `MainWindow::add_document_file()` through a queued Qt call. This keeps the socket communication separate from the GUI thread and lets me reuse the document-addition logic already implemented. Because of this, no change was necessary to the search widgets or processing backend. Also, the remote additions invalidate the current corpus just like adding a document normally would.

5. Fault Switches and Tests
Both fault switches are in `check_build_completion()`.
    - `AIWS_FAULT_ACCEPT_STALE` is caught by `reject_stale_build()` and `reset_rejects_old_build()`.
    - `AIWS_FAULT_LOSE_FAILURE` is caught by `build_failure_is_reported()`.
All 8 normal test results passed and both fault switches caused the expected test failures within 60 seconds.

6. AI-Use Disclosure
I used AI, ChatGPT, to help me better understand requirements and how to implement certain parts of the project. It helped with threading concepts, background corpus building, and how they work together. I used it on files such as `main_window.cpp`, `ipc_server.cpp`, and `main.cpp` for figuring out how to approach the edits or formulation of the files. It also helped with developing test cases for the `student_tests.cpp` file, especially for checking stale builds and failed builds as this is not my strongest skill. I reviewed the suggested code snippets, made necessary changes, and tested everything in WSL Ubuntu. Overall, AI definitely helped with understanding implementation and why certain changes were necessary.