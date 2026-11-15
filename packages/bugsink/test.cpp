#define SLIMR_TEST_MAIN
#include "test.hpp"
#include "bugsink.hpp"
#include <sstream>

using namespace slimr;

TEST_SUITE("slimr::bugsink") {
    TEST_CASE("DSN parsing") {
        auto parsed = BugsinkReporter::parseDsn("https://8973845e128c4204ba7a7e02eaba6a17@bugsink.btek.cc/4");
        CHECK(parsed.valid == true);
        CHECK(parsed.protocol == "https");
        CHECK(parsed.publicKey == "8973845e128c4204ba7a7e02eaba6a17");
        CHECK(parsed.host == "bugsink.btek.cc");
        CHECK(parsed.projectId == "4");

        BugsinkReporter reporter("https://key123@example.com/99");
        CHECK(reporter.envelopeUrl() == "https://example.com/api/99/envelope/?sentry_key=key123&sentry_version=7");
    }

    TEST_CASE("Envelope NDJSON formatting") {
        BugsinkReporter reporter("https://pub@sentry.local/1");
        Json payload = Json::object_t{};
        payload["message"] = "test event";

        std::string env = reporter.formatEnvelope(payload, "event123", "2026-11-10T12:00:00Z");

        // Verify it contains exactly 3 newline-separated JSON objects
        std::vector<std::string> lines;
        std::stringstream ss(env);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty())
                lines.push_back(line);
        }

        CHECK(lines.size() == 3);
        CHECK(lines[0].find("\"event_id\":\"event123\"") != std::string::npos);
        CHECK(lines[0].find("\"sent_at\":\"2026-11-10T12:00:00Z\"") != std::string::npos);
        CHECK(lines[1] == "{\"type\":\"event\",\"content_type\":\"application/json\"}");
        CHECK(lines[2].find("\"message\":\"test event\"") != std::string::npos);
    }

    TEST_CASE("Environment override disables on empty string") {
        setenv("TEST_BUGSINK_DSN", "", 1);
        BugsinkReporter reporter("https://pub@sentry.local/1", "TEST_BUGSINK_DSN");
        CHECK(reporter.effectiveDsn() == "");
        CHECK(reporter.envelopeUrl() == "");
        unsetenv("TEST_BUGSINK_DSN");
    }

    TEST_CASE("Release parameter is included when non-empty and omitted when empty") {
        BugsinkReporter withRelease("https://pub@sentry.local/1", "BUGSINK_DSN", "app@1.2.3");
        CHECK(withRelease.release() == "app@1.2.3");
        auto errPayload = withRelease.buildErrorPayload(std::runtime_error("boom"), "ev1", "2026-11-15T00:00:00Z");
        CHECK(errPayload["release"].get<std::string>() == "app@1.2.3");
        auto msgPayload = withRelease.buildMessagePayload("hi", "info", "ev2", "2026-11-15T00:00:00Z");
        CHECK(msgPayload["release"].get<std::string>() == "app@1.2.3");

        BugsinkReporter withoutRelease("https://pub@sentry.local/1");
        CHECK(withoutRelease.release().empty());
        auto errNoRel = withoutRelease.buildErrorPayload(std::runtime_error("boom"), "ev3", "2026-11-15T00:00:00Z");
        CHECK(!errNoRel.contains("release"));
        auto msgNoRel = withoutRelease.buildMessagePayload("hi", "info", "ev4", "2026-11-15T00:00:00Z");
        CHECK(!msgNoRel.contains("release"));
    }
}
