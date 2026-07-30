July 22, 2026:

Let's shift focus and make a free list allocator that I can use for strings in
the ui system.


July 21, 2026:

Clipping implemented roughly (without color vertex interpolation). I fixed a
design mistake where we fix out of bounds children within parent on non-layout
direction even if strictness was 1. Next, I want to brainstorm solid ways to 
implement scroll offsets. It would be awesome once dropdowns are implemented to
have a debug key that shows ui element information in a tooltip. Maybe pressing
a key can show the currently displayed element's parent.


July 20, 2026:

I want to implement element clipping by adjusting out of bounds children's 
screen coords, texture source coords, and manipulating vertex colors. This
probably should be done after autolayout and before drawing.


July 19, 2026:

I need to implement scrollable or offset children within ui elements so that
I can autoscroll textfields that extend beyond the bounds. I should brainstorm
several ways to implement this and consider their qualities before I start
implementing anything. Also, currently the cursor grows faster than typed text, 
so I need to fix that.

## Motivation is fleeting, discipline gets things done. I should write down my
goals so that I can work towards them.
