struct Flags {
    // for some commands, will sleep for sleepMs milliseconds
    // before proceeding
    int sleepMs = 0;

    Flags() = default;
    ~Flags();
};
