# M5 Design Note

1. M4 Fixes
None of my M4 features needed major bug fixes. However, I changed how background builds work. M4 used a `QTimer` to check for completion, while M5 uses one persistent worker thread that notifies the GUI when it finishes. This also makes cancellation and shutdown easier to handle.

2. Threads and Shared Data
`MainWindow` and all GUI widgets stay on the Qt main thread. The build worker runs `worker_loop()` and uses a copy of the Workspace. The separate `IpcServer` thread from M4 handles remote document requests.

Following Constraint 4, every piece of mutable data shared between threads is protected by a mutex or uses an atomic variable. `worker_mutex_` protects `pending_request_` and `stop_worker_`, which the GUI and worker both access. `cancellation_epoch_` is atomic because both threads read it and the GUI updates it. The request's promise and `build_future_` transfer results safely. `IpcServer` also uses its atomic `stopping_` flag for shutdown. Workspace data, State objects, and widgets stay on the GUI thread.

3. State Pattern
I implemented four State classes to handle the corpus lifecycle.

State           Meaning               Events and Transitions

EmptyState      No documents          Add → Dirty

DirtyState      Documents exist,      Build → Building;
                corpus outdated       Reset → Empty

BuildingState   Build in progress     Success → Ready;
                                      Failure or Add → Dirty;
                                      Reset → Empty

ReadyState      Corpus is current     Build → Building;
                                      Add → Dirty;
                                      Reset → Empty

Events go through `dispatch_state_event()`, and each State object decides what happens. `update_action_states()` uses these objects to control button availability. Exit ends the application from any state.

4. Event Sequence
One situation that could cause problems is resetting during a long build and immediately starting another.

1. The GUI calls `build_corpus()`, which reaches `start_build_request()` and places a request in `pending_request_`.

2. The worker wakes through `worker_cv_` and begins processing its Workspace copy.

3. The user presses `Reset`. `reset_application()` increments `cancellation_epoch_`, clears the workspace, and changes the state to `Empty`.

4. The worker notices the changed counter and stops processing the outdated build.

5. The user adds another document and requests a new build. The worker processes the newer request.

6. After finishing, the worker notifies the GUI through `QMetaObject::invokeMethod()`. `check_build_completion()` accepts the current result and changes `Building` to `Ready`.

This prevents outdated work from delaying or replacing the newer corpus.


5. Three Design Decisions
Build during a build: The `Building` state does not accept another `Build` request. `BuildingState::request_build()` leaves the running build unchanged.

Remote add mid-build: `IpcServer` forwards the request to `add_document_file()`, which cancels outdated work and invalidates the corpus. The GUI returns to `Dirty`, and the user can rebuild.

Shared data: I kept shared data limited to the pending request, shutdown flag, and cancellation counter. `worker_loop()` uses a Workspace snapshot so the worker does not need direct access to GUI data.


6. Fault Switches and Tests
Both switches are in `main_window.cpp`. `AIWS_FAULT_NO_CANCEL` disables cancellation checks in `worker_loop()` and is caught by `cancelled_build_does_not_delay_replacement()`.

`AIWS_FAULT_SILENT_SHUTDOWN` skips `worker_cv_.notify_one()` in the destructor. The tests detect this through a 30-second shutdown timeout.

All eight tests pass normally, and both faults cause the expected failures.

7. AI-Use Disclosure

I used ChatGPT to better understand threading, cancellation, and the State pattern. It helped me review the implementation of main_window.cpp, develop verification tests, and organize this design note, specifically with writing down the correct classes and functions. I reviewed the code suggestions it made, made edits to them, and tested everything in WSL Ubuntu to make sure everything was correct.