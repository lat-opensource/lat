# Contributing to LAT / 参与 LAT 贡献

## Fork and pull request workflow / Fork 与 Pull Request 工作流

LAT uses a fork-first contribution workflow. Create ordinary feature, fix,
documentation, and experiment branches in your personal fork, even if you have
write access to `lat-opensource/lat`. Keep the upstream repository for its
maintained branches and for exceptional work, such as release, stable, or
emergency branches, that has been explicitly approved or coordinated by the
maintainers.

A typical setup is shown below. Replace `YOUR_ACCOUNT` and `TOPIC_BRANCH` with
your GitHub account and a descriptive branch name.

```console
git clone https://github.com/YOUR_ACCOUNT/lat.git
cd lat
git remote add upstream https://github.com/lat-opensource/lat.git
git fetch upstream
git switch -c TOPIC_BRANCH upstream/master
git push -u origin TOPIC_BRANCH
```

Then open a pull request from `YOUR_ACCOUNT:TOPIC_BRANCH` to
`lat-opensource/lat:master`. Start each unrelated change on a new branch based
on the current upstream target branch; do not reuse an already merged branch.

LAT 默认采用 fork-first 贡献流程。即使你拥有 `lat-opensource/lat` 的写入
权限，普通功能、修复、文档和实验分支也应创建在个人 fork 中。上游仓库仅
保留项目维护分支，以及经过维护者明确批准或协调的特殊工作，例如发布、
稳定版或紧急修复分支。

典型设置方式如下。请将 `YOUR_ACCOUNT` 和 `TOPIC_BRANCH` 分别替换为你的
GitHub 账号和具有描述性的分支名称：

```console
git clone https://github.com/YOUR_ACCOUNT/lat.git
cd lat
git remote add upstream https://github.com/lat-opensource/lat.git
git fetch upstream
git switch -c TOPIC_BRANCH upstream/master
git push -u origin TOPIC_BRANCH
```

随后从 `YOUR_ACCOUNT:TOPIC_BRANCH` 向 `lat-opensource/lat:master` 创建
Pull Request。每项无关变更都应从上游最新目标分支新建独立分支；不要重复
使用已经合并的分支。

## DCO / Commit sign-off

Every commit must include a Developer Certificate of Origin (DCO) sign-off. By
signing off, you certify the statements in
[DCO 1.1](https://developercertificate.org/), including that you have the right
to submit the contribution under the project's open source license.

Create a signed-off commit with:

```console
git commit -s -m "your commit message"
```

The `-s` option adds a trailer like this to the commit message:

```text
Signed-off-by: Name <email@example.com>
```

The name and email in `Signed-off-by` must match the commit author. A DCO
sign-off is an author attestation and is different from GPG or SSH
cryptographic commit signing.

Automation and AI assistants must not add `Signed-off-by` without your explicit
confirmation and authorization. You must review the contribution, take
responsibility for it, and explicitly authorize the signed commit or commits.

To add a sign-off to the latest commit:

```console
git commit --amend --signoff
git push --force-with-lease origin HEAD
```

The push is needed only if the original commit was already pushed. To add
sign-offs to multiple commits on a branch, replace `TARGET_BRANCH` with the
pull request's target branch:

```console
git fetch upstream
git rebase --signoff upstream/TARGET_BRANCH
git push --force-with-lease origin HEAD
```

The command adds your sign-off to every rebased commit. Use it only when you
authored or otherwise have the right to certify every commit being rebased, and
do not remove existing sign-offs. Replace `origin` with the name of your fork
remote if it differs. Rebasing rewrites commit history, so coordinate with
anyone else using the branch before rebasing or force-pushing it.

每个提交都必须包含开发者原创声明（Developer Certificate of Origin，DCO）
签署。签署表示你确认 [DCO 1.1](https://developercertificate.org/) 中的声明，
包括你有权按照项目的开源许可证提交该贡献。

请使用以下命令创建带签署的提交：

```console
git commit -s -m "提交说明"
```

`-s` 选项会在提交说明末尾添加类似下面的内容：

```text
Signed-off-by: 姓名 <email@example.com>
```

`Signed-off-by` 中的姓名和邮箱必须与提交作者一致。DCO 签署是作者对提交
来源和授权的声明，与 GPG 或 SSH 的密码学提交签名不同。

未经你明确确认和授权，自动化工具和 AI 助手不得添加 `Signed-off-by`。
你必须审阅贡献、承担责任，并明确授权需要签署的一个或多个提交。

如果最新的提交忘记签署，可以执行：

```console
git commit --amend --signoff
git push --force-with-lease origin HEAD
```

只有原提交已经推送到远端时才需要再次推送。如果分支上的多个提交都
需要补充签署，请将 `TARGET_BRANCH` 替换为 Pull Request 的目标分支后执行：

```console
git fetch upstream
git rebase --signoff upstream/TARGET_BRANCH
git push --force-with-lease origin HEAD
```

该命令会为所有被 rebase 的提交添加你的签署。只有当每个提交都是你
创作的，或者你有权对其作出认证时，才能使用该命令；同时不要删除已有
签署。如果你的个人 fork 远端不叫 `origin`，请替换为实际名称。rebase 会
改写提交历史，因此在 rebase 或强制推送前，请先与同样使用该分支的其他
贡献者协调。

## Before opening a pull request / 提交 Pull Request 前

- Keep each change focused and follow the relevant
  [commit convention](COMMIT_CONVENTION.en.md).
- Build or test the affected functionality and describe the validation in the
  pull request, or explain why validation is not applicable.
- Confirm that every commit in the pull request contains a `Signed-off-by`
  trailer.

- 请保持每项变更内容集中，并遵循相关的
  [提交规范](COMMIT_CONVENTION.md)。
- 请构建或测试受影响的功能并在 Pull Request 中说明结果；如不适用，请说明
  原因。
- 请确认 Pull Request 中的每个提交都包含 `Signed-off-by`。
