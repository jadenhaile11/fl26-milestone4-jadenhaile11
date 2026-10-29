# M4 Design Note

Due Wed Oct 28, 11:59 pm. See specification Section 7.

1. Your components and the thread each one's functions run on.

I plan to keep `MainWindow` and the M3 widgets on the GUI thread so they can handle user actions and display results. A separate `std::thread` will build the corpus using a copy of the `Workspace` data. A `QTimer` will check a `std::future` to see when the build finishes. The worker thread will not directly change any widgets.

2. Your corpus states and the events that move between them.

The four states will be `Unavailable`, `Building`, `Current`, and `Failed`. Clicking `Build Corpus` starts a background build. The corpus only becomes `Current` if the Workspace has not changed. Adding documents or clicking `Reset` makes the corpus `Unavailable`. If a build fails, the application displays an error. `Search`, `Context`, and `Term Inspector` can only use a `Current` corpus. Only one worker will run at a time.

3. The event sequences that threaten R1 to R4 and how your design handles each, with two of them walked through step by step.

For R1, running builds in the background keeps the GUI responsive. For R2, I will use a Workspace revision number to reject outdated results. For R3, errors will be reported back to the GUI. For R4, I will make sure threads are properly joined before they are destroyed.

Example 1 - Adding a document during a build:

1. A build starts at revision 4.
2. The user adds a document, changing the revision to 5.
3. When the build finishes, the revisions do not match, so the result is discarded.
4. The user can rebuild using the updated Workspace.

Example 2 - Reset during a build:

1. The user clicks `Reset` while a build is running.
2. The Workspace is cleared and its revision increases.
3. The old build may finish, but its result will be ignored.
4. Once the worker is cleaned up, a new build can start. 

4. The ownership path of one built corpus, from creation to destruction.

The worker creates and owns a temporary `ProcessingCore` during the build. Once finished, the result is passed back through the future. `MainWindow` either accepts the result or discards it if the Workspace changed. I will use `std::unique_ptr` to manage ownership and avoid manual deletion.

5. One alternative you rejected, and why.

I considered using `processEvents()` to keep the GUI responsive while rebuilding. However, this would still run the build on the GUI thread and could cause problems if the user interacts with the application during the build. A separate worker thread is a better approach because it keeps the GUI responsive and makes it easiesr to manage the results.