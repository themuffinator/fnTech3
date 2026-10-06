/* fnTech3 Player Guide: theme switch, guide drawer, copy buttons, the
 * "on this page" highlight and the heading search. The pages read fully
 * without it; scripts/player_manual.py copies it into manual/assets/.
 */
(function () {
  "use strict";

  var root = document.documentElement;
  var THEME_KEY = "fntech3-manual-theme";

  function systemTheme() {
    return window.matchMedia && window.matchMedia("(prefers-color-scheme: light)").matches
      ? "light" : "dark";
  }

  var themeToggle = document.querySelector(".theme-toggle");
  if (themeToggle) {
    themeToggle.addEventListener("click", function () {
      var next = (root.dataset.theme || systemTheme()) === "dark" ? "light" : "dark";
      root.dataset.theme = next;
      try {
        localStorage.setItem(THEME_KEY, next);
      } catch (error) {
        /* Private windows may refuse storage; the switch still applies. */
      }
    });
  }

  /* The guide list becomes a drawer on narrow screens. */
  var navToggle = document.querySelector(".nav-toggle");
  function setNav(open) {
    document.body.classList.toggle("nav-open", open);
    if (navToggle) {
      navToggle.setAttribute("aria-expanded", open ? "true" : "false");
    }
  }
  if (navToggle) {
    navToggle.addEventListener("click", function () {
      setNav(!document.body.classList.contains("nav-open"));
    });
    document.addEventListener("click", function (event) {
      if (document.body.classList.contains("nav-open")
          && !event.target.closest(".sidebar") && !event.target.closest(".nav-toggle")) {
        setNav(false);
      }
    });
  }

  /* Copy buttons on examples. */
  function copyText(text) {
    if (navigator.clipboard && window.isSecureContext) {
      return navigator.clipboard.writeText(text);
    }
    return new Promise(function (resolve, reject) {
      var area = document.createElement("textarea");
      area.value = text;
      area.setAttribute("readonly", "");
      area.style.position = "fixed";
      area.style.top = "-1000px";
      document.body.appendChild(area);
      area.select();
      var copied = false;
      try {
        copied = document.execCommand("copy");
      } catch (error) {
        copied = false;
      }
      document.body.removeChild(area);
      if (copied) {
        resolve();
      } else {
        reject(new Error("copy refused"));
      }
    });
  }

  Array.prototype.forEach.call(document.querySelectorAll(".copy-button"), function (button) {
    var label = button.querySelector("span");
    var timer = 0;
    button.addEventListener("click", function () {
      var code = button.closest(".code-block").querySelector("code");
      function settle(text, done) {
        label.textContent = text;
        button.classList.toggle("done", done);
        window.clearTimeout(timer);
        timer = window.setTimeout(function () {
          label.textContent = "Copy";
          button.classList.remove("done");
        }, 1800);
      }
      copyText(code.textContent).then(function () {
        settle("Copied", true);
      }, function () {
        var range = document.createRange();
        range.selectNodeContents(code);
        var selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(range);
        settle("Press Ctrl+C", false);
      });
    });
  });

  /* Highlight the section being read in "On this page". */
  var tocLinks = Array.prototype.slice.call(document.querySelectorAll(".toc a"));
  var tocTargets = tocLinks.map(function (link) {
    return document.getElementById(decodeURIComponent(link.hash.slice(1)));
  });
  var spyQueued = false;
  function updateSpy() {
    spyQueued = false;
    var active = -1;
    for (var i = 0; i < tocTargets.length; i++) {
      if (tocTargets[i] && tocTargets[i].getBoundingClientRect().top < 130) {
        active = i;
      }
    }
    tocLinks.forEach(function (link, i) {
      link.classList.toggle("active", i === active);
    });
  }
  if (tocLinks.length) {
    window.addEventListener("scroll", function () {
      if (!spyQueued) {
        spyQueued = true;
        window.requestAnimationFrame(updateSpy);
      }
    }, { passive: true });
    updateSpy();
  }

  /* Search the headings of every guide. */
  var input = document.getElementById("search-input");
  var list = document.getElementById("search-results");
  var entries = window.FNTECH3_MANUAL_INDEX || [];
  var results = [];
  var selected = -1;

  function closeResults() {
    if (list) {
      list.hidden = true;
      input.setAttribute("aria-expanded", "false");
    }
    selected = -1;
  }

  function select(index) {
    var items = list.querySelectorAll("li[role='option']");
    selected = index;
    Array.prototype.forEach.call(items, function (item, i) {
      item.setAttribute("aria-selected", i === index ? "true" : "false");
      if (i === index) {
        item.scrollIntoView({ block: "nearest" });
      }
    });
  }

  function search(query) {
    var terms = query.toLowerCase().split(/\s+/).filter(Boolean);
    list.textContent = "";
    selected = -1;
    if (!terms.length) {
      closeResults();
      return;
    }
    /* A section matches when every word is in its title, its guide's name or
       the settings and commands it mentions. Title matches come first, then
       exact setting names, then the rest. */
    function rank(entry) {
      var title = entry.t.toLowerCase();
      if (terms.every(function (term) { return title.indexOf(term) !== -1; })) {
        return 0;
      }
      var names = (entry.k || "").toLowerCase().split(" ");
      return terms.some(function (term) { return names.indexOf(term) !== -1; }) ? 1 : 2;
    }
    function matchedName(entry) {
      var names = (entry.k || "").split(" ");
      for (var i = 0; i < names.length; i++) {
        var name = names[i].toLowerCase();
        for (var j = 0; j < terms.length; j++) {
          if (name && name.indexOf(terms[j]) !== -1) {
            return names[i];
          }
        }
      }
      return "";
    }
    results = entries.filter(function (entry) {
      var haystack = (entry.t + " " + entry.g + " " + (entry.k || "")).toLowerCase();
      return terms.every(function (term) {
        return haystack.indexOf(term) !== -1;
      });
    }).map(function (entry, order) {
      return { entry: entry, rank: rank(entry), order: order };
    });
    results.sort(function (a, b) {
      return a.rank - b.rank || a.order - b.order;
    });
    results = results.slice(0, 14).map(function (item) {
      return item.rank === 0 ? { t: item.entry.t, g: item.entry.g, h: item.entry.h, m: "" }
        : { t: item.entry.t, g: item.entry.g, h: item.entry.h, m: matchedName(item.entry) };
    });
    if (!results.length) {
      var empty = document.createElement("li");
      empty.className = "search-empty";
      empty.textContent = "Nothing matches. Try fewer or different words.";
      list.appendChild(empty);
    }
    results.forEach(function (entry, i) {
      var item = document.createElement("li");
      item.setAttribute("role", "option");
      item.id = "search-result-" + i;
      var link = document.createElement("a");
      link.href = entry.h;
      var title = document.createElement("span");
      title.textContent = entry.t;
      var guide = document.createElement("span");
      guide.className = "result-guide";
      guide.textContent = entry.g;
      if (entry.m) {
        var setting = document.createElement("code");
        setting.textContent = entry.m;
        guide.appendChild(document.createTextNode(" · "));
        guide.appendChild(setting);
      }
      link.appendChild(title);
      link.appendChild(guide);
      item.appendChild(link);
      list.appendChild(item);
    });
    list.hidden = false;
    input.setAttribute("aria-expanded", "true");
  }

  if (input && list) {
    input.setAttribute("aria-controls", "search-results");
    input.setAttribute("aria-expanded", "false");
    input.addEventListener("input", function () {
      search(input.value);
    });
    input.addEventListener("keydown", function (event) {
      if (event.key === "ArrowDown" && results.length) {
        event.preventDefault();
        select(Math.min(selected + 1, results.length - 1));
      } else if (event.key === "ArrowUp" && results.length) {
        event.preventDefault();
        select(Math.max(selected - 1, 0));
      } else if (event.key === "Enter" && results.length) {
        event.preventDefault();
        window.location.href = results[selected >= 0 ? selected : 0].h;
        closeResults();
      } else if (event.key === "Escape") {
        input.value = "";
        closeResults();
        input.blur();
      }
    });
    input.addEventListener("focus", function () {
      if (input.value) {
        search(input.value);
      }
    });
    document.addEventListener("click", function (event) {
      if (!event.target.closest(".search")) {
        closeResults();
      }
    });
  }

  document.addEventListener("keydown", function (event) {
    var target = event.target;
    var typing = target && (target.isContentEditable || /^(input|textarea|select)$/i.test(target.tagName));
    if (event.key === "/" && !typing && input) {
      event.preventDefault();
      input.focus();
      input.select();
    } else if (event.key === "Escape" && document.body.classList.contains("nav-open")) {
      setNav(false);
      if (navToggle) {
        navToggle.focus();
      }
    }
  });
})();
