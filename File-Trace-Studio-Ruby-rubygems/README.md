# File-Trace-Studio Ruby SDK

`gem install file-trace-studio`

Windows x64 Ruby3.1+.

```ruby
require "file_trace_studio"
FileTraceStudioSDK::Client.open { |client| p client.call("status") }
```

[API and operation schema](docs/API.md). Matching driver installed separately.
