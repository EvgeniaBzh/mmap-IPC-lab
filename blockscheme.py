import sys
sys.path.insert(0, "/home/claude/diagrams")
from drawio_builder import Diagram

d = Diagram("Block-scheme - Recommendation Algorithm", page_w=1500, page_h=1820)

TERM = "rounded=1;arcSize=50;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;fontSize=13;fontStyle=1;"
PROC = "rounded=0;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;fontSize=12;"
SUBPROC = "shape=process;whiteSpace=wrap;html=1;backgroundOutline=1;darkOpacity=0.05;fillColor=#f8cecc;strokeColor=#b85450;fontSize=12;"
DEC = "rhombus;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;fontSize=11;"
IO = "shape=parallelogram;perimeter=parallelogramPerimeter;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;fontSize=12;"
FLOW = "html=1;endArrow=block;endFill=1;strokeColor=#000000;fontSize=11;rounded=0;"

d.vertex("title", "Блок-схема алгоритму обчислення та ранжування рекомендацій", "text;html=1;fontStyle=5;fontSize=17;align=left;fillColor=none;strokeColor=none;", 30, 0, 900, 30)

CX = 510
SW = 260  # spine width
SX = CX - SW // 2
RX = 830  # right branch column
RW = 260

start = d.vertex("start", "Початок", TERM, CX - 90, 50, 180, 50)
io1 = d.vertex("io1", "Ввести: U (інгредієнти\nкористувача), фільтри", IO, SX, 140, SW, 60)
p1 = d.vertex("p1", "Знайти рецепти-кандидати\nчерез обернений індекс\n«інгредієнт → рецепти»", SUBPROC, SX, 240, SW, 80)
d1 = d.vertex("d1", "Список\nкандидатів\nпорожній?", DEC, SX + 30, 360, 200, 100)

io_empty = d.vertex("io_empty", "Вивести:\n«Рецептів не знайдено»", IO, RX, 385, RW, 60)

p2 = d.vertex("p2", "i := 1;  S_list := ∅", PROC, SX, 510, SW, 55)
d2 = d.vertex("d2", "i ≤ |Candidates|?", DEC, SX + 30, 605, 200, 90)

p3 = d.vertex("p3", "Обчислити m(rᵢ) = |I(rᵢ)∩U| / |I(rᵢ)|\n(формула 1)", PROC, SX, 735, SW, 65)
d3 = d.vertex("d3", "Фільтри\nзадані?", DEC, SX + 30, 840, 200, 90)

p4 = d.vertex("p4", "Обчислити предикат\nInclude(rᵢ) (формула 4)", PROC, RX, 862, RW, 65)
d4 = d.vertex("d4", "Include(rᵢ) = 1?", DEC, RX + 30, 955, 200, 90)
p_skip = d.vertex("p_skip", "i := i + 1", PROC, RX + 260, 985, 180, 55)

p5 = d.vertex("p5", "Обчислити Missing(rᵢ), d(rᵢ)\n(формула 2)", PROC, SX, 970, SW, 65)
p6 = d.vertex("p6", "Обчислити S(rᵢ) = w1·m(rᵢ)+w2·norm(rating)+\n+w3·norm(pop)−w4·norm(d)  (формула 5)", PROC, SX - 20, 1075, SW + 40, 70)
p7 = d.vertex("p7", "Додати (rᵢ, S(rᵢ)) до S_list;\ni := i + 1", PROC, SX, 1185, SW, 65)

p_sort = d.vertex("p_sort", "Відсортувати S_list за\nспаданням S(rᵢ)", PROC, SX, 1310, SW, 65)
p_page = d.vertex("p_page", "Взяти перші 20 елементів\n(пагінація, FR3)", PROC, SX, 1415, SW, 60)
io2 = d.vertex("io2", "Вивести: ранжований\nперелік рекомендацій", IO, SX, 1515, SW, 60)
end = d.vertex("end", "Кінець", TERM, CX - 90, 1615, 180, 50)

# flows
d.edge("f1", "start", "io1", style=FLOW)
d.edge("f2", "io1", "p1", style=FLOW)
d.edge("f3", "p1", "d1", style=FLOW)

d.edge("f4", "d1", "io_empty", value="так", style=FLOW, exit_x=1, exit_y=0.5, entry_x=0, entry_y=0.5)
d.edge("f5", "io_empty", "end", style=FLOW, exit_x=1, exit_y=0.5, entry_x=1, entry_y=0.5,
       points=[(1250, 415), (1250, 1640)])

d.edge("f6", "d1", "p2", value="ні", style=FLOW, exit_x=0.5, exit_y=1, entry_x=0.5, entry_y=0)
d.edge("f7", "p2", "d2", style=FLOW)

d.edge("f8", "d2", "p3", value="так", style=FLOW, exit_x=0.5, exit_y=1, entry_x=0.5, entry_y=0)
d.edge("f9", "p3", "d3", style=FLOW)

d.edge("f10", "d3", "p4", value="так", style=FLOW, exit_x=1, exit_y=0.5, entry_x=0, entry_y=0.5)
d.edge("f11", "p4", "d4", style=FLOW)
d.edge("f12", "d4", "p_skip", value="ні", style=FLOW, exit_x=1, exit_y=0.5, entry_x=0, entry_y=0.5)
d.edge("f13", "p_skip", "d2", style=FLOW, exit_x=0.5, exit_y=0, entry_x=1, entry_y=0.5,
       points=[(1160, 800), (760, 650)])
d.edge("f14", "d4", "p5", value="так", style=FLOW, exit_x=0.5, exit_y=1, entry_x=1, entry_y=0.3,
       points=[(760, 1085)])

d.edge("f15", "d3", "p5", value="ні", style=FLOW, exit_x=0.5, exit_y=1, entry_x=0.5, entry_y=0)

d.edge("f16", "p5", "p6", style=FLOW)
d.edge("f17", "p6", "p7", style=FLOW)
d.edge("f18", "p7", "d2", style=FLOW, exit_x=0, exit_y=0.5, entry_x=0, entry_y=0.5,
       points=[(280, 1217), (280, 650)])

d.edge("f19", "d2", "p_sort", value="ні", style=FLOW, exit_x=1, exit_y=0.5, entry_x=1, entry_y=0,
       points=[(950, 650), (950, 1250), (SX + SW, 1342)])

d.edge("f20", "p_sort", "p_page", style=FLOW)
d.edge("f21", "p_page", "io2", style=FLOW)
d.edge("f22", "io2", "end", style=FLOW)

d.save("/mnt/user-data/outputs/diagrams/07_block_scheme.drawio")