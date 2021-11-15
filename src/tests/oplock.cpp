#include "test.h"

using namespace std;

#define FSCTL_REQUEST_OPLOCK_LEVEL_2 CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 1, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FILE_OPLOCK_BROKEN_TO_LEVEL_2       0x00000007
#define FILE_OPLOCK_BROKEN_TO_NONE          0x00000008

static unique_handle req_oplock_level2(HANDLE h, IO_STATUS_BLOCK& iosb) {
    NTSTATUS Status;
    HANDLE ev;

    Status = NtCreateEvent(&ev, MAXIMUM_ALLOWED, nullptr, NotificationEvent, false);
    if (Status != STATUS_SUCCESS)
        throw ntstatus_error(Status);

    Status = NtFsControlFile(h, ev, nullptr, nullptr, &iosb,
                             FSCTL_REQUEST_OPLOCK_LEVEL_2,
                             nullptr, 0, nullptr, 0);

    if (Status != STATUS_PENDING)
        throw ntstatus_error(Status);

    return unique_handle(ev);
}

static bool check_event(HANDLE h) {
    NTSTATUS Status;
    EVENT_BASIC_INFORMATION ebi;

    Status = NtQueryEvent(h, EventBasicInformation, &ebi, sizeof(ebi), nullptr);

    if (Status != STATUS_SUCCESS)
        throw ntstatus_error(Status);

    return ebi.EventState;
}

void test_oplocks(const u16string& dir) {
    unique_handle h, h2;

    test("Create file", [&]() {
        h = create_file(dir + u"\\oplock1", FILE_READ_DATA, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        FILE_CREATE, 0, FILE_CREATED);
    });

    if (h) {
        unique_handle ev;
        IO_STATUS_BLOCK iosb;

        test("Get level 2 oplock", [&]() {
            ev = req_oplock_level2(h.get(), iosb);
        });

        if (ev) {
            test("Check oplock not broken", [&]() {
                if (check_event(ev.get()))
                    throw runtime_error("Oplock is broken");
            });

            test("Open second handle (FILE_OPEN)", [&]() {
                h2 = create_file(dir + u"\\oplock1", FILE_READ_DATA, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 FILE_OPEN, 0, FILE_OPENED);
            });

            test("Check oplock still not broken", [&]() {
                if (check_event(ev.get()))
                    throw runtime_error("Oplock is broken");
            });

            h2.reset();

            // FIXME - test level 2 oplock broken if file reopened with FILE_RESERVE_OPFILTER

            test("Open second handle (FILE_SUPERSEDE)", [&]() {
                h2 = create_file(dir + u"\\oplock1", FILE_READ_DATA, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 FILE_SUPERSEDE, 0, FILE_SUPERSEDED);
            });

            test("Check oplock broken", [&]() {
                if (!check_event(ev.get()))
                    throw runtime_error("Oplock is not broken");

                if (iosb.Information != FILE_OPLOCK_BROKEN_TO_NONE)
                    throw formatted_error("iosb.Information was {}, expected FILE_OPLOCK_BROKEN_TO_NONE", iosb.Information);
            });

            h2.reset();

            test("Get level 2 oplock", [&]() {
                ev = req_oplock_level2(h.get(), iosb);
            });

            test("Check oplock not broken", [&]() {
                if (check_event(ev.get()))
                    throw runtime_error("Oplock is broken");
            });

            test("Open second handle (FILE_OVERWRITE)", [&]() {
                h2 = create_file(dir + u"\\oplock1", FILE_READ_DATA, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 FILE_OVERWRITE, 0, FILE_OVERWRITTEN);
            });

            test("Check oplock broken", [&]() {
                if (!check_event(ev.get()))
                    throw runtime_error("Oplock is not broken");

                if (iosb.Information != FILE_OPLOCK_BROKEN_TO_NONE)
                    throw formatted_error("iosb.Information was {}, expected FILE_OPLOCK_BROKEN_TO_NONE", iosb.Information);
            });

            h2.reset();

            test("Get level 2 oplock", [&]() {
                ev = req_oplock_level2(h.get(), iosb);
            });

            test("Check oplock not broken", [&]() {
                if (check_event(ev.get()))
                    throw runtime_error("Oplock is broken");
            });

            test("Open second handle (FILE_OVERWRITE_IF)", [&]() {
                h2 = create_file(dir + u"\\oplock1", FILE_READ_DATA, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 FILE_OVERWRITE_IF, 0, FILE_OVERWRITTEN);
            });

            test("Check oplock broken", [&]() {
                if (!check_event(ev.get()))
                    throw runtime_error("Oplock is not broken");

                if (iosb.Information != FILE_OPLOCK_BROKEN_TO_NONE)
                    throw formatted_error("iosb.Information was {}, expected FILE_OPLOCK_BROKEN_TO_NONE", iosb.Information);
            });

            h2.reset();
        }

        h.reset();
    }

    // FIXME - test no level 2 oplock granted for directory
    // FIXME - test no level 2 oplock granted if synchronous
    // FIXME - test no level 2 oplock granted if any byte-range locks
    // FIXME - test granted if level 2 / read oplocks already there
    // FIXME - test not granted (STATUS_OPLOCK_NOT_GRANTED) if any other sort of oplock there

    // FIXME - read
    // FIXME - write
    // FIXME - lock
    // FIXME - FileEndOfFileInformation
    // FIXME - FileAllocationInformation
    // FIXME - FileValidDataLengthInformation
    // FIXME - FileRenameInformation
    // FIXME - FileLinkInformation
    // FIXME - FileDispositionInformation
    // FIXME - FSCTL_SET_ZERO_DATA

    // FIXME - FILE_RESERVE_OPFILTER
    // FIXME - FILE_OPEN_REQUIRING_OPLOCK
    // FIXME - FILE_COMPLETE_IF_OPLOCKED
}