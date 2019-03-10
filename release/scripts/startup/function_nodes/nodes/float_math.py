import bpy
from bpy.props import *
from .. base import FunctionNode

operation_items = [
    ("ADD", "Add", "", "", 1),
    ("MULTIPLY", "Multiply", "", "", 2),
    ("MIN", "Minimum", "", "", 3),
    ("MAX", "Maximum", "", "", 4),
]

class FloatMathNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_FloatMathNode"
    bl_label = "Float Math"

    operation: EnumProperty(
        name="Operation",
        items=operation_items,
    )

    def get_sockets(self):
        return [
            ("fn_FloatSocket", "A"),
            ("fn_FloatSocket", "B"),
        ], [
            ("fn_FloatSocket", "Result"),
        ]

    def draw(self, layout):
        layout.prop(self, "operation", text="")

bpy.utils.register_class(FloatMathNode)