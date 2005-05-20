# The data for the pie chart
set data {25 18 15 12 8 30 35}
    
# The labels for the pie chart
set labels {"Labor" "Licenses" "Taxes" "Legal"
            "Insurance" "Facilities" "Production"}

# Create a PieChart object of size 300 x 230 pixels
set chart [ns_chartdir create pie 300 230]

# Set the background color of the chart to gold (goldGradient). Use a 2
# pixel 3D border.
ns_chartdir setbackground $chart [ns_chartdir gradientcolor $chart goldGradient] -1 2

# Set the center of the pie at (150 115) and the radius to 80 pixels
ns_chartdir pie $chart setpiesize 150 115 80

# Add a title box using 10 point Arial Bold font. Set the background
# color to red metallic (redMetalGradient). Use a 1 pixel 3D border.
ns_chartdir addtitle $chart "Pie Chart Coloring Demo" Top "arialbd.ttf" 10 TextColor \
            [ns_chartdir gradientcolor $chart redMetalGradient] -1 1

# Draw the pie in 3D
ns_chartdir pie $chart set3d

# Set the pie data and the pie labels
ns_chartdir pie $chart setdata $data $labels

# output the chart in PNG format
ns_chartdir save $chart "goldpie.png"

# destroy the chart to free up resources
ns_chartdir destroy $chart

