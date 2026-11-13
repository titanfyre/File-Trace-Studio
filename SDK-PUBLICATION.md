# SDK publication status

Current source layout and registry coverage. A GitHub download is distinct from publication on a central registry.

| Target | Status | Package / documentation |
| --- | --- | --- |
| CPP/vcpkg | published_custom_registry | file-trace-studio |
| CPP/conan | public_recipe_builds_locally |  [Source](packaging/native/) |
| CPP/nuget | paused_pending_owner_email_confirmation | File-Trace-Studio.Native |
| NET/nuget | built_verified_publication_on_hold_by_owner | FileTraceStudio.SDK [Source](File-Trace-Studio-NET-nuget/) |
| Python/pypi | built_fixture_and_production_verified | file-trace-studio [Source](File-Trace-Studio-Python-pypi/) |
| Python/conda-forge | built_tested_requires_source_publication_and_official_review | file-trace-studio [Source](packaging/conda/) |
| Rust/cargo | published_install_verified | [file-trace-studio](https://crates.io/crates/file-trace-studio) [Source](File-Trace-Studio-Rust-cargo/) |
| JavaScript/npm | awaiting_browser_publication_authentication | [file-trace-studio](https://www.npmjs.com/package/file-trace-studio) [Source](File-Trace-Studio-JavaScript-npm/) |
| Java/maven | published_install_verified | [file-trace-studio](https://central.sonatype.com/artifact/io.github.titancsa/file-trace-studio/1.0.0) [Source](File-Trace-Studio-Java-maven/) |
| Java/jitpack | published_install_verified | [file-trace-studio](https://jitpack.io/#titanfyre/File-Trace-Studio/java-sdk-v1.0.0) [Source](File-Trace-Studio-Java-maven/) |
| Go/modules | published_install_verified | [file-trace-studio](https://pkg.go.dev/github.com/titanfyre/File-Trace-Studio/File-Trace-Studio-Go-modules@v1.0.0) [Source](File-Trace-Studio-Go-modules/) |
| Go/pkg-go-dev | indexed_verified | [Documentation](https://pkg.go.dev/github.com/titanfyre/File-Trace-Studio/File-Trace-Studio-Go-modules@v1.0.0) |
| PowerShell/gallery | published_install_verified | [FileTraceStudio](https://www.powershellgallery.com/packages/FileTraceStudio/1.0.0) [Source](File-Trace-Studio-PowerShell-gallery/) |
| Dart/pub-dev | built_fixture_and_production_verified | file_trace_studio [Source](File-Trace-Studio-Dart-pub-dev/) |
| Ruby/rubygems | published_install_verified | [file-trace-studio](https://rubygems.org/gems/file-trace-studio) [Source](File-Trace-Studio-Ruby-rubygems/) |
| PHP/packagist | composer_source_and_tag_published | [titancsa/file-trace-studio](https://packagist.org/packages/titancsa/file-trace-studio) |

Native SDK source and CMake/Conan/vcpkg/NuGet recipes share [`packaging/native`](packaging/native/LAYOUT.md). Previous release archives and tags remain unchanged.

Application and driver prerequisites and original licensing remain those documented by each SDK. No driver installer is bundled with these source packages.
