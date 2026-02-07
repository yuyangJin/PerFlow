# PerFlow Documentation Style Guide

This guide ensures consistency across all PerFlow documentation.

## General Guidelines

### Tone and Voice
- **Clear and direct**: Use simple, straightforward language
- **Professional but friendly**: Accessible to both beginners and experts
- **Action-oriented**: Focus on what users can do
- **Avoid jargon**: Define technical terms when first used

### Target Audiences
- **Users**: Application developers using PerFlow for profiling
- **Developers**: Contributors extending or modifying PerFlow
- **Operators**: System administrators deploying PerFlow

## Document Structure

### File Naming
- Use UPPERCASE for major documents: `README.md`, `GETTING_STARTED.md`
- Use descriptive names: `CONFIGURATION.md` not `CONFIG.md`
- Separate words with underscores: `TIMER_BASED_SAMPLER.md`

### Headers
```markdown
# Document Title (H1 - once per document)

## Major Section (H2)

### Subsection (H3)

#### Detail Level (H4)
```

### Table of Contents
Add for documents >200 lines:
```markdown
## Table of Contents
- [Section 1](#section-1)
- [Section 2](#section-2)
```

## Formatting Conventions

### Code Blocks

**Bash commands:**
```bash
# Include descriptive comment
mkdir -p /tmp/samples
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
mpirun -n 4 ./your_app
```

**C++ code:**
```cpp
#include "analysis/online_analysis.h"

int main() {
    OnlineAnalysis analysis;
    // Clear, commented examples
    return 0;
}
```

**Output examples:**
```
# Use # for command prompt
# rank_0.pflw  rank_1.pflw  rank_2.pflw  rank_3.pflw
```

### Emphasis

- **Bold** for important terms, UI elements, file names
- *Italic* for emphasis or introducing terms
- `Code` for commands, variables, file paths, function names

### Lists

**Unordered lists:**
```markdown
- First item
- Second item
  - Nested item
  - Another nested
```

**Ordered lists:**
```markdown
1. Step one
2. Step two
3. Step three
```

### Links

**Internal links:**
```markdown
[Getting Started](user-guide/GETTING_STARTED.md)
```

**External links:**
```markdown
[GitHub Issues](https://github.com/yuyangJin/PerFlow/issues)
```

**Anchor links:**
```markdown
[Jump to section](#configuration)
```

### Tables

```markdown
| Column 1 | Column 2 | Column 3 |
|----------|----------|----------|
| Value 1  | Value 2  | Value 3  |
```

Align columns for readability in source.

## Special Elements

### Callouts

Use emoji for visual callouts:

```markdown
💡 **Tip**: Helpful suggestions

⚠️ **Warning**: Important cautions

📝 **Example**: Usage examples

🔍 **Advanced**: Advanced topics

✅ **Best Practice**: Recommended approaches

❌ **Avoid**: Things to avoid
```

### Examples

Always provide complete, working examples:

```markdown
**Example: Basic Usage**
```bash
# Complete command that users can copy-paste
LD_PRELOAD=lib/libperflow_mpi_sampler_timer.so \
PERFLOW_OUTPUT_DIR=/tmp/samples \
mpirun -n 4 ./my_app
```
```

### Command Syntax

Show required vs. optional parameters:

```markdown
# Required parameters in CAPS, optional in [brackets]
LD_PRELOAD=LIBRARY_PATH \
PERFLOW_OUTPUT_DIR=OUTPUT_DIR \
[PERFLOW_SAMPLING_FREQ=FREQUENCY] \
mpirun -n NUM_RANKS ./APPLICATION
```

## Content Guidelines

### Prerequisites Section
List requirements clearly:
```markdown
### Required
- Item 1
- Item 2

### Optional
- Item 3
- Item 4
```

### Installation Instructions
1. Start with system dependencies
2. Show commands for major platforms (Ubuntu, RHEL, macOS)
3. Include verification steps

### Configuration Options
Use consistent format:
```markdown
#### OPTION_NAME
**Type**: Data type  
**Required**: Yes/No  
**Default**: Default value  
**Range**: Valid range  
**Description**: What it does

```bash
OPTION_NAME=value
```
```

### Troubleshooting
Structure as problem → solution:
```markdown
### Problem: Brief description

**Symptoms**:
- Observable behavior

**Solution**:
1. Step-by-step fix
```

### Code Examples
- Complete, working examples
- Include necessary headers/imports
- Add explanatory comments
- Show expected output

## Language and Grammar

### Voice
- Use active voice: "Configure the sampler" not "The sampler should be configured"
- Use second person: "You can profile..." not "Users can profile..."
- Use present tense: "PerFlow provides..." not "PerFlow will provide..."

### Consistency
- "PerFlow" (not "perflow", "Perflow", "PERFLOW")
- "MPI" (all caps, not "Mpi")
- "API" not "Api"
- Use American English spelling

### Common Terms
- **sampling frequency** (not "sample rate")
- **call stack** (not "callstack" or "call-stack")
- **overhead** (not "over-head")
- **hotspot** (one word)
- **setup** (noun), **set up** (verb)

## File Organization

### User Guides (docs/user-guide/)
- Getting started
- Configuration
- Usage guides
- Troubleshooting

### Developer Guides (docs/developer-guide/)
- Architecture
- Implementation details
- Contributing
- Internal APIs

### API Reference (docs/api-reference/)
- Complete API documentation
- Technical specifications
- Reference material

## Version Control

### Commit Messages
```
[Docs] Brief description

- Detailed change 1
- Detailed change 2
```

### PR Titles
```
[Docs] Update configuration guide with new options
```

## Testing Documentation

Before committing:

1. **Verify all links work** - Check internal and external links
2. **Test all code examples** - Ensure they compile and run
3. **Check formatting** - Use markdown linter
4. **Spell check** - Run spell checker
5. **Review accuracy** - Verify against actual code

## Documentation Types

### README Files
- Overview of component/directory
- Quick navigation
- Essential information only

### Tutorials
- Step-by-step instructions
- Complete, working examples
- Progressive complexity

### How-To Guides
- Task-oriented
- Assume some knowledge
- Focus on specific goals

### Reference Documentation
- Complete, accurate
- Systematic coverage
- Technical details

### Troubleshooting Guides
- Problem-solution format
- Common issues first
- Links to detailed docs

## Maintenance

### Regular Updates
- Review quarterly for accuracy
- Update with new features
- Archive obsolete content
- Fix broken links

### Version Information
Include version when behavior differs:
```markdown
**Note**: This feature requires PerFlow 2.0+
```

### Deprecation Notices
```markdown
⚠️ **Deprecated**: This option is deprecated in favor of `NEW_OPTION`. It will be removed in version 3.0.
```

## Examples of Good Documentation

### Good Example
```markdown
## Configuration

### PERFLOW_SAMPLING_FREQ
**Type**: Integer (Hz)  
**Default**: 1000  
**Range**: 100-10000

Sampling frequency in samples per second. Higher values provide more detail but increase overhead.

**Example**:
```bash
PERFLOW_SAMPLING_FREQ=2000  # 2kHz sampling
```

💡 **Tip**: Start with 1000 Hz for balanced performance.
```

### Poor Example
```markdown
## Config

Set freq:
```bash
PERFLOW_SAMPLING_FREQ=2000
```

Higher = more detail but slower.
```

## Checklist for New Documentation

- [ ] Clear title and purpose
- [ ] Target audience identified
- [ ] Prerequisites listed
- [ ] Working code examples
- [ ] Consistent formatting
- [ ] Internal links tested
- [ ] External links verified
- [ ] Spell checked
- [ ] Grammar checked
- [ ] Reviewed by peer

## Resources

- [Markdown Guide](https://www.markdownguide.org/)
- [Google Developer Documentation Style Guide](https://developers.google.com/style)
- [Microsoft Writing Style Guide](https://docs.microsoft.com/en-us/style-guide/)

---

For questions about this style guide, open an issue on GitHub.
