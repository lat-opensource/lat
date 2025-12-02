# Git 提交规范（基于历史提交总结）

为了使代码有良好的可追溯性，根据项目现有提交记录补充如下规范。

## 标题基础格式
- **推荐结构**：`<范围>[, <类型>]: <简洁描述>`。
- **范围（Scope）**：模块或子项目名称（如 `LATX`、`lat`、`KZT`、`linux-user`），与路径或子系统保持一致。
- **类型（Type）**：常用 `feat`（新特性）、`fix`（修复）、`opt`（优化）；特殊场景使用 `temporary fix`（临时修复）、`Build(deps)`（依赖或 CI 维护）。
- **描述（Subject）**：一句话概括改动，祈使/陈述语气的简短英文描述，避免句号结尾。

## Type 约定（可按需扩充）
| Type             | 含义 / 适用场景                                    | 示例标题片段                       |
| ---------------- | -------------------------------------------------- | ---------------------------------- |
| `LATX, infra`    | 对基础设施、构建系统、CI/CD等不影响产品功能的修改  | `LATX, infra: Add tracing for ...` |
| `LATX, feat`     | 对产品面向业务的功能增加                           | `LATX, feat: introduce smc ...` |
| `LATX, fix`      | bug 修复                                           | `LATX, fix: Handle null ...`       |
| `LATX, opt`      | 性能或实现优化                                     | `LATX, opt: Refine glue ...`       |
| `LATX, refactor` | 函数 / 功能重构                                    | `LATX, refactor: Split ...`        |
| `LATX, style`    | 不影响功能的格式、注释调整                         | `LATX, style: Reflow comments`     |
| `LATX-X64, *`    | 改动仅针对 64 位，`*` 同上各类（如 fix）           | `LATX-X64, fix: ...`               |
| `linux-user`     | linux-user 相关（如 syscall 改动）                 | `linux-user, fix: ...`             |
| `LATX, docs`     | 文档                                               | `LATX, docs: Update convention`    |
| 其他历史类型     | 特殊场景下也可用 `temporary fix`、`Build(deps)` 等 | `Build(deps): Bump ...`            |

> 范围与类型之间用逗号分隔，类型与描述之间用冒号；无类型时直接用冒号，例如 `cpus: Make {start,end}_exclusive() recursive`。

### `feat` vs `infra` 区分指南

#### `feat` ——  功能级新增（Feature）

** 只要本次修改让翻译器新增了可感知的能力或行为”，就是 `feat`。 **

典型场景：
- 新增 API、指令翻译、指令支持
- 新增命令行参数、功能开关
- 新增业务逻辑或用户可看到的行为

** 判断标准：**
> “如果写进 release note，用户会在意吗？”
** 会 → feat  **

---

#### `infra` ——  工程基础设施（Infrastructure）

** 工程环境、构建系统、CI/CD、工具链、开发脚本等方面的改动，不改变产品行为。**

典型场景：
- 修改 Makefile / CMake / Meson / Bazel
- 开发者自用的追踪或分析工具等

** 判断标准：**
> “用户是否能感知行为变化？”
** 不能 → infra  **

## 标题示例（可直接套用）
- `LATX, feat: Support CONFIG_LATX_GLUE_MASK in indirect jmp glue`
- `LATX, opt: Reduce thread contention in fast path`
- `Build(deps): Update meson to 1.3.0`

## commit body 要求
1. **建议保留正文**：除 `style`、`docs` 类型，或极其简单且首行即可描述清楚的提交外，不提倡无正文的提交。
2. **fix 类型需指明修复对象**：正文中写明修复了哪个应用程序/测试集或 git issue 等，便于追溯。
3. **正文内容建议**：
   - 补充背景、动机、方案与验证方式，不必在标题展开细节。
   - 单个提交聚焦单一主题，避免一次提交多个无关改动。
   - 对临时修复或风险点，正文说明影响范围、替代方案与清理计划，方便后续跟进。

## 提交前检查清单
- 标题不宜过长，描述末尾不加句号。
- 是否包含范围且范围名称与子系统一致？
- 类型是否符合场景，是否需要 `temporary fix` 或 `Build(deps)` 等特殊标识？
- 描述是否清晰指向改动 / 原因，避免只有模糊词（如 “update code”、“fix bug”）。
- 是否按上述要求填写 commit body（尤其对 `fix` 类型）？
- 若为临时方案或有后续计划，正文是否注明 TODO / 后续跟进？
