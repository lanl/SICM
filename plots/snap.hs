import Graphics.Rendering.Chart.Easy
import Graphics.Rendering.Chart.Backend.Cairo

type HorzType = Double
type VertType = Double

sizes :: [HorzType]
sizes = map ((/(2^20)) . (*8))
  [ 13107200, 25600000, 52000000, 104857600, 209920000, 416160000
  , 838860800, 1679360000, 3362249600
  ]

nopin :: [VertType]
nopin = map (/1000)
  [ 4996, 9485, 19406, 37091, 75217, 148865, 314076, 663451, 1445799
  ]

pin :: [VertType]
pin = map (/1000)
  [ 2838, 6412, 19406, 24499, 47915, 104569, 230719, 472778, 951144
  ]

normalpage :: [VertType]
normalpage = map (/1000)
  [ 2783, 5692, 19406, 23382, 45460, 96693, 217401, 441222, 878991
  ]

hugepage :: [VertType]
hugepage = map (/1000)
  [ 3010, 6434, 19406, 23534, 45644, 99015, 217354, 441072
  ]

main =
  toFile ((\(FileOptions sz _) -> FileOptions sz PDF) def) "snap.pdf" $ do
    layout_title .= "Scaling of Modified SNAP"
    layout_x_axis . laxis_title .= "Idealized Size (MiB)"
    layout_y_axis . laxis_title .= "Time (s)"
    plot (line "intrinsic nopin" [zip sizes nopin])
    plot (line "intrinsic pin" [zip sizes pin])
    plot (line "sicm normal pages" [zip sizes normalpage])
    plot (line "sicm huge pages" [zip sizes hugepage])
