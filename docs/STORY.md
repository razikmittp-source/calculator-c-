# the story of mark

This is what's known. Some of it comes from his README, some from the
`.txt` files left in the repository's `notebooks/` folder before he
deleted them, some from his sister, who replied to one email in 2024.

---

## before

His name was Mark. He was twenty-eight when he wrote core-c. He had
worked at three small companies in five years — the kind of companies
that build dashboards for other companies. He was not remarkable as a
programmer. He was, by every account, kind, quiet, a careful reviewer
of other people's code.

He lived with someone for four years. She was a translator. She is
not named in any commit, anywhere, on purpose. He referred to her in
his notes only as *"the one i kept writing letters to."*

In March 2023 she died. He took the rest of that week off, then the
rest of that month, then the rest of the year. His employer was
patient at first, then less so, then not at all. By August he was
unemployed and living on what was left of two paychecks and a small
inheritance.

He stopped sleeping at normal hours. He stopped going outside during
daylight. He started writing core-c.

---

## the language, written at night

He worked on it from roughly 11pm to 4am every night for fourteen
months. He kept a notebook open next to the keyboard and wrote
sentences in it whenever he didn't know what code to write. Many of
these sentences ended up as keywords:

> *"i don't want to write `if`. i want to write `perhaps`. nothing has been certain in nine months."*

> *"while is too clean. it sounds like a clock. what i'm doing is `drown`. that's the verb."*

> *"return is what programs do. fade is what people do."*

He wanted a language where leaving was a deliberate word, where
failure didn't shout, where the names of types were names of things
that hurt or were missing. He called the integer type `memory`
because *"that's what numbers are: things you keep counting until you
can't anymore."* He called the float type `pain` because *"integers
couldn't hold it."*

He gave the language no exception handling because, in his words,
*"there is no exception handler for missing her, and it felt dishonest
to pretend the computer had one for anything else."* Instead, when
something fails — a missing variable, a division by zero, an
out-of-range index — the value becomes `void`, the program keeps
going, and the failure is added to a list called the **tomb**. The
tomb is written to disk after the program ends, as
`<source>.crc.tomb`. You can read it, or not. The program never knew
it was failing.

The editor came later, in the last three months. He wrote it in C++
with GTK3 because *"that's what was already on the laptop and i
didn't have it in me to install anything new."* He called it `abyss`.
He made the theme dark, removed the toggle, and added a dialog that
appears if anyone clicks the placeholder *"light theme"* button:

> *"there is no light here. mark removed the switch. it was the first thing he committed."*

He did, in fact, remove it in his first commit.

---

## the release

On April 14, 2024, at 4:17 a.m., he pushed a single commit titled
*"first."* to a public GitHub repository. The README at that point
was three lines:

```
core-c.
a small language.
i'm sorry it isn't more.
```

He posted the link to one programming forum, with the words *"i made
this. i don't think it does much."* and then went to sleep.

He didn't post anywhere else. He didn't open any pull requests. He
didn't reply to the first comments.

---

## what people did with it

The forum thread grew slowly for two days, then quickly for the next
two weeks. Most of the early replies were about the syntax — the fact
that `if` was `perhaps`, that `while` was `drown`, that the type for
floats was called `pain`. A few people said it was tasteless. More
people said it was honest. A handful of people started writing small
programs in it: a tarot drawer, a journal-entry tracker, a script
that emailed you a single line from a poem at the same time every
night.

Someone (not him) wrote a syntax-highlighter for VS Code. Someone
wrote a vim plugin. Someone in Brazil ported the interpreter to Lua.
Someone in Berlin gave a fifteen-minute talk at a small conference
titled *"a language about grief"* and didn't ask for permission,
which mark — when he eventually read about it — said he didn't mind.

By June 2024 the repository had about four thousand stars. He had
not committed again.

---

## what happened to him

He sent one email, in July 2024, to his sister. She has shared one
sentence of it, with permission:

> *"i'm okay. the language was the part i wanted to finish."*

He removed the contact links from the README in October 2024 — a
commit titled simply *"quieter."* It's the second commit in the
project's history, and so far the last.

He has not been on the forum since. He has not pushed any code. His
public profile shows the same single repository, with the same two
commits.

His sister has said he is alive, that he is working in a town none
of them grew up in, that he asked her not to share more than that.
She added, *"he wants the language to stand on its own. he doesn't
want to be the language."*

---

## what the community did next

Several people maintain forks. There's a small Discord, which the
moderators have agreed not to advertise — anyone who finds it finds
it. They keep a rule: no one is allowed to talk about mark in detail.
The pinned message reads:

> *the project is the artifact. the person is not.*

The interpreter you are reading now is a small clean-room
reimplementation of his original — same syntax, same semantics, same
silent failure model, same `tomb`. It exists so that the language
keeps working without depending on the original repository, in case
that repository ever goes private, or quiet, or `void`.

---

*last edited: an unspecified evening, by no one in particular.*
