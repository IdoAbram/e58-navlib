# Claude Code Guidance

- Keep each class declaration and implementation in its own matching header and source file.
- Prefix data members with `m_`.
- Prefix private functions with `_`, static functions with `s_`, and private static functions with `_s_`.
- Format C++ code with the repository's `.clang-format` configuration.
- Include the specific public header needed by each consumer.